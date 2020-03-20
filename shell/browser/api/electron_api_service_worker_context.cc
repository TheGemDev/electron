// Copyright (c) 2019 Slack Technologies, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/api/electron_api_service_worker_context.h"

#include <string>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "content/public/browser/console_message.h"
#include "content/public/browser/storage_partition.h"
#include "gin/data_object_builder.h"
#include "gin/handle.h"
#include "shell/browser/electron_browser_context.h"
#include "shell/common/gin_converters/value_converter.h"
#include "shell/common/gin_helper/dictionary.h"
#include "shell/common/gin_helper/object_template_builder.h"
#include "shell/common/node_includes.h"

namespace electron {

namespace api {

namespace {

std::string MessageSourceToString(
    const blink::mojom::ConsoleMessageSource source) {
  if (source == blink::mojom::ConsoleMessageSource::kXml)
    return "xml";
  if (source == blink::mojom::ConsoleMessageSource::kJavaScript)
    return "javascript";
  if (source == blink::mojom::ConsoleMessageSource::kNetwork)
    return "network";
  if (source == blink::mojom::ConsoleMessageSource::kConsoleApi)
    return "console-api";
  if (source == blink::mojom::ConsoleMessageSource::kStorage)
    return "storage";
  if (source == blink::mojom::ConsoleMessageSource::kAppCache)
    return "app-cache";
  if (source == blink::mojom::ConsoleMessageSource::kRendering)
    return "rendering";
  if (source == blink::mojom::ConsoleMessageSource::kSecurity)
    return "security";
  if (source == blink::mojom::ConsoleMessageSource::kDeprecation)
    return "deprecation";
  if (source == blink::mojom::ConsoleMessageSource::kWorker)
    return "worker";
  if (source == blink::mojom::ConsoleMessageSource::kViolation)
    return "violation";
  if (source == blink::mojom::ConsoleMessageSource::kIntervention)
    return "intervention";
  if (source == blink::mojom::ConsoleMessageSource::kRecommendation)
    return "recommendation";
  return "other";
}

v8::Local<v8::Value> ServiceWorkerRunningInfoToDict(
    v8::Isolate* isolate,
    const content::ServiceWorkerRunningInfo& info) {
  return gin::DataObjectBuilder(isolate)
      .Set("scriptUrl", info.script_url.spec())
      .Set("scope", info.scope.spec())
      .Set("renderProcessId", info.render_process_id)
      .Build();
}

}  // namespace

ServiceWorkerContext::ServiceWorkerContext(
    v8::Isolate* isolate,
    ElectronBrowserContext* browser_context)
    : browser_context_(browser_context), weak_ptr_factory_(this) {
  Init(isolate);
  service_worker_context_ =
      content::BrowserContext::GetDefaultStoragePartition(browser_context_)
          ->GetServiceWorkerContext();
  service_worker_context_->AddObserver(this);
}

ServiceWorkerContext::~ServiceWorkerContext() {
  service_worker_context_->RemoveObserver(this);
}

void ServiceWorkerContext::OnReportConsoleMessage(
    int64_t version_id,
    const content::ConsoleMessage& message) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  Emit("console-message",
       gin::DataObjectBuilder(isolate)
           .Set("versionId", version_id)
           .Set("source", MessageSourceToString(message.source))
           .Set("level", static_cast<int32_t>(message.message_level))
           .Set("message", message.message)
           .Set("lineNumber", message.line_number)
           .Set("sourceUrl", message.source_url.spec())
           .Build());
}

void ServiceWorkerContext::OnDestruct(content::ServiceWorkerContext* context) {
  if (context == service_worker_context_) {
    delete this;
  }
}

v8::Local<v8::Value> ServiceWorkerContext::GetAllRunningWorkerInfo(
    v8::Isolate* isolate) {
  gin::DataObjectBuilder builder(isolate);
  const base::flat_map<int64_t, content::ServiceWorkerRunningInfo>& info_map =
      service_worker_context_->GetRunningServiceWorkerInfos();
  for (auto iter = info_map.begin(); iter != info_map.end(); ++iter) {
    builder.Set(
        std::to_string(iter->first),
        ServiceWorkerRunningInfoToDict(isolate, std::move(iter->second)));
  }
  return builder.Build();
}

v8::Local<v8::Value> ServiceWorkerContext::GetWorkerInfoFromID(
    gin_helper::ErrorThrower thrower,
    int64_t version_id) {
  const base::flat_map<int64_t, content::ServiceWorkerRunningInfo>& info_map =
      service_worker_context_->GetRunningServiceWorkerInfos();
  auto iter = info_map.find(version_id);
  if (iter == info_map.end()) {
    thrower.ThrowError("Could not find service worker with that version_id");
    return v8::Local<v8::Value>();
  }
  return ServiceWorkerRunningInfoToDict(thrower.isolate(),
                                        std::move(iter->second));
}

// static
gin::Handle<ServiceWorkerContext> ServiceWorkerContext::Create(
    v8::Isolate* isolate,
    ElectronBrowserContext* browser_context) {
  return gin::CreateHandle(isolate,
                           new ServiceWorkerContext(isolate, browser_context));
}

// static
void ServiceWorkerContext::BuildPrototype(
    v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(gin::StringToV8(isolate, "ServiceWorkerContext"));
  gin_helper::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
      .SetMethod("getAllRunning",
                 &ServiceWorkerContext::GetAllRunningWorkerInfo)
      .SetMethod("getFromVersionID",
                 &ServiceWorkerContext::GetWorkerInfoFromID);
}

}  // namespace api

}  // namespace electron