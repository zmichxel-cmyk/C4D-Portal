#include <napi.h>
#include "virtual_camera.h"

// N-API bindings exposed to the Electron main process as:
//   const cam = require('./build/Release/c4dportal_virtual_camera.node');
//   cam.create('C4D Portal');
//   cam.start();
//   cam.pushFrame(buffer, width, height); // buffer: BGRA Uint8Array
//   cam.stop();
namespace {

VirtualCamera g_camera;

Napi::Value Create(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  std::string name = info.Length() > 0 ? info[0].As<Napi::String>().Utf8Value() : "C4D Portal";
  std::wstring wname(name.begin(), name.end());
  std::wstring error;
  bool ok = g_camera.Create(wname, &error);
  if (!ok) {
    std::string narrowError(error.begin(), error.end());
    Napi::Error::New(env, narrowError).ThrowAsJavaScriptException();
    return env.Null();
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value Start(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  std::wstring error;
  bool ok = g_camera.Start(&error);
  if (!ok) {
    std::string narrowError(error.begin(), error.end());
    Napi::Error::New(env, narrowError).ThrowAsJavaScriptException();
    return env.Null();
  }
  return Napi::Boolean::New(env, true);
}

Napi::Value PushFrame(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 3 || !info[0].IsBuffer()) {
    Napi::TypeError::New(env, "pushFrame(buffer, width, height)").ThrowAsJavaScriptException();
    return env.Null();
  }
  auto buf = info[0].As<Napi::Buffer<uint8_t>>();
  uint32_t width = info[1].As<Napi::Number>().Uint32Value();
  uint32_t height = info[2].As<Napi::Number>().Uint32Value();
  return Napi::Boolean::New(env, g_camera.PushFrame(buf.Data(), buf.Length(), width, height));
}

Napi::Value Stop(const Napi::CallbackInfo& info) {
  g_camera.Stop();
  return info.Env().Undefined();
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("create", Napi::Function::New(env, Create));
  exports.Set("start", Napi::Function::New(env, Start));
  exports.Set("pushFrame", Napi::Function::New(env, PushFrame));
  exports.Set("stop", Napi::Function::New(env, Stop));
  return exports;
}

}  // namespace

NODE_API_MODULE(c4dportal_virtual_camera, Init)
