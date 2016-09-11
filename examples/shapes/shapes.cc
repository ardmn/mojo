// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <string>
#include <utility>

#include <magenta/process.h>
#include <magenta/syscalls.h>
#include <mojo/system/main.h>

#include "mojo/public/cpp/application/application_impl_base.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/run_application.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/services/framebuffer/interfaces/framebuffer.mojom.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace {

class ShapesApp : public mojo::ApplicationImplBase {
 public:
  ShapesApp() {}
  ~ShapesApp() override {}

  void OnInitialize() override {
    mojo::ConnectToService(shell(), "mojo:framebuffer",
                           mojo::GetProxy(&provider_));
    CreateFramebuffer();
  }

  void CreateFramebuffer() { provider_->Create(CallCreateSurface(this)); }

  void CreateSurface(mojo::InterfaceHandle<mojo::Framebuffer> framebuffer,
                     mojo::FramebufferInfoPtr info) {
    if (!framebuffer) {
      fprintf(stderr, "Failed to create framebuffer\n");
      return;
    }
    framebuffer_.Bind(std::move(framebuffer));
    info_ = std::move(info);

    uintptr_t buffer = 0;
    size_t row_bytes = info_->row_bytes;
    size_t size = row_bytes * info_->size->height;

    mx_status_t status = mx_process_map_vm(
        mx_process_self(), info_->vmo.get().value(), 0, size, &buffer,
        MX_VM_FLAG_PERM_READ | MX_VM_FLAG_PERM_WRITE);

    if (status < 0) {
      fprintf(stderr, "Cannot map framebuffer %d\n", status);
      mojo::RunLoop::current()->Quit();
      return;
    }

    SkColorType sk_color_type;
    switch (info_->format) {
      case mojo::FramebufferFormat::RGB_565:
        sk_color_type = kRGB_565_SkColorType;
        break;
      case mojo::FramebufferFormat::ARGB_8888:
        sk_color_type = kRGBA_8888_SkColorType;
        break;
      default:
        printf("Unknown color type %d\n", info_->format);
        sk_color_type = kRGB_565_SkColorType;
        break;
    }
    SkImageInfo image_info =
        SkImageInfo::Make(info_->size->width, info_->size->height,
                          sk_color_type, kPremul_SkAlphaType);

    surface_ = SkSurface::MakeRasterDirect(
        image_info, reinterpret_cast<void*>(buffer), row_bytes);

    mojo::RunLoop::current()->PostDelayedTask([this]() { Draw(); }, 0);
  }

  void Draw() {
    x_ += 10.0f;
    if (x_ > info_->size->width)
      x_ = 0.0;

    SkCanvas* canvas = surface_->getCanvas();
    canvas->clear(SK_ColorBLUE);
    SkPaint paint;
    paint.setColor(0xFFFF00FF);
    paint.setAntiAlias(true);
    canvas->drawCircle(x_, y_, 200.0f, paint);
    canvas->flush();

    framebuffer_->Flush([this]() { Draw(); });
  }

 private:
  class CallCreateSurface {
   public:
    explicit CallCreateSurface(ShapesApp* app) : app_(app) {}

    void Run(mojo::InterfaceHandle<mojo::Framebuffer> framebuffer,
             mojo::FramebufferInfoPtr info) const {
      app_->CreateSurface(std::move(framebuffer), std::move(info));
    }

   private:
    ShapesApp* app_;
  };

  mojo::FramebufferProviderPtr provider_;
  mojo::FramebufferPtr framebuffer_;
  mojo::FramebufferInfoPtr info_;
  sk_sp<SkSurface> surface_;
  float x_ = 400.0f;
  float y_ = 300.0f;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ShapesApp);
};

}  // namespace

MojoResult MojoMain(MojoHandle request) {
  ShapesApp app;
  return mojo::RunApplication(request, &app);
}
