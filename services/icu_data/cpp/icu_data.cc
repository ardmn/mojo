// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/icu_data/cpp/icu_data.h"

#include <magenta/syscalls.h>

#include "mojo/public/cpp/application/connect.h"
#include "mojo/services/icu_data/cpp/constants.h"
#include "mojo/services/icu_data/interfaces/icu_data.mojom.h"
#include "third_party/icu/source/common/unicode/udata.h"

namespace icu_data {
namespace {

static uintptr_t g_icu_data_ptr = 0u;

// Helper function. Given a VMO handle, map the memory into the process and
// return a pointer to the memory.
// |size_out| is required and is set with the size of the mapped memory
// region.
uintptr_t GetDataFromVMO(mx_handle_t vmo, uint64_t* size_out) {
  if (!size_out)
    return 0u;
  mx_status_t status = mx_vmo_get_size(vmo, size_out);
  if (status != NO_ERROR || *size_out > std::numeric_limits<mx_size_t>::max())
    return 0u;

  uintptr_t data = 0u;
  status = mx_process_map_vm(mx_process_self(), vmo, 0, *size_out, &data,
                             MX_VM_FLAG_PERM_READ);
  if (status == NO_ERROR)
    return data;

  return 0u;
}

}  // namespace

// Initialize ICU data
//
// Connects to ICU Data Provider (a mojo service) and requests the ICU data.
// Then, initializes ICU with the data received.
//
// Return value indicates if initialization was successful.
bool Initialize(mojo::ApplicationConnector* application_connector) {
  if (g_icu_data_ptr) {
    // Don't allow calling Initialize twice.
    return false;
  }

  // Get the data from the ICU data provider.
  mojo::ICUDataProviderPtr icu_data_provider;
  ConnectToService(application_connector, "mojo:icu_data_provider",
                   GetProxy(&icu_data_provider));

  mojo::ICUDataPtr response;
  icu_data_provider->ICUDataWithSha1(
      kDataHash, [&response](mojo::ICUDataPtr r) { response = std::move(r); });
  icu_data_provider.WaitForIncomingResponse();

  if (!response) {
    return false;
  }

  uint64_t data_size = 0;
  uintptr_t data = GetDataFromVMO(response->vmo.get().value(), &data_size);

  // Pass the data to ICU.
  if (data) {
    UErrorCode err = U_ZERO_ERROR;
    udata_setCommonData(reinterpret_cast<const char*>(data), &err);
    g_icu_data_ptr = data;
    return err == U_ZERO_ERROR;
  } else {
    Release();
  }

  return false;
}

// Release mapped ICU data
//
// If Initialize() was called earlier, unmap the ICU data we had previously
// mapped to this process. ICU cannot be used after calling this method.
//
// Return value indicates if unmapping data was successful.
bool Release() {
  if (g_icu_data_ptr) {
    // Unmap the ICU data.
    mx_status_t status =
        mx_process_unmap_vm(mx_process_self(), g_icu_data_ptr, 0);
    g_icu_data_ptr = 0u;
    return status == NO_ERROR;
  } else {
    return false;
  }
}

}  // namespace icu_data
