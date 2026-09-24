// SPDX-License-Identifier: GPL-2.0-or-later
// UI-only task scheduler. Owned, joined workers replace Microsoft PPL on MinGW.
#include "vstgui/lib/platform/win32/win32taskexecutor.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace VSTGUI {
namespace {
class Worker {
public:
  Worker():thread([this]{run();}){}
  ~Worker() {
    {std::lock_guard lock(mutex);stopping=true;}
    wake.notify_one();thread.join();
  }
  void schedule(Tasks::Task task) {
    {std::lock_guard lock(mutex);tasks.push_back(std::move(task));++pending;}
    wake.notify_one();
  }
  std::atomic<unsigned> pending{0};
private:
  void run() {
    for(;;) {
      Tasks::Task task;
      {std::unique_lock lock(mutex);wake.wait(lock,[this]{return stopping || !tasks.empty();});
       if(tasks.empty())return;task=std::move(tasks.front());tasks.pop_front();}
      try {task();} catch(...) {OutputDebugStringW(L"OrganVST UI task failed\n");}
      task={};--pending;
    }
  }
  std::mutex mutex;
  std::condition_variable wake;
  std::deque<Tasks::Task> tasks;
  bool stopping=false;
  std::thread thread;
};
}
struct Win32TaskExecutor::Impl {
  Tasks::Queue main{0},background{1};
  HINSTANCE instance{};HWND window{};ATOM atom{};
  std::wstring className;
  std::atomic<unsigned> mainPending{0};
  std::mutex mutex;
  std::map<uint64_t,std::shared_ptr<Worker>> workers{{1,std::make_shared<Worker>()}};
  uint64_t next=2;
  const std::thread::id owner=std::this_thread::get_id();
  static constexpr UINT message=WM_APP+41;
  std::shared_ptr<Worker> worker(uint64_t id) {
    std::lock_guard lock(mutex);auto found=workers.find(id);
    return found==workers.end()?nullptr:found->second;
  }
  static LRESULT CALLBACK dispatch(HWND window,UINT msg,WPARAM w,LPARAM l) {
    auto* self=reinterpret_cast<Impl*>(GetWindowLongPtrW(window,GWLP_USERDATA));
    if(self && msg==message) {
      std::unique_ptr<Tasks::Task> task(reinterpret_cast<Tasks::Task*>(w));
      try {(*task)();} catch(...) {OutputDebugStringW(L"OrganVST main UI task failed\n");}
      task.reset();--self->mainPending;return 0;
    }
    return DefWindowProcW(window,msg,w,l);
  }
  void pump() {
    MSG msg{};
    if(std::this_thread::get_id()==owner && PeekMessageW(&msg,window,message,message,PM_REMOVE))
      DispatchMessageW(&msg);
    else std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
};
Win32TaskExecutor::Win32TaskExecutor(HINSTANCE instance):impl(std::make_unique<Impl>()) {init(instance);}
void Win32TaskExecutor::init(HINSTANCE instance) {
  if(impl->window)return;
  impl->instance=instance;
  impl->className=L"OrganVST-VSTGUI-Tasks-"+std::to_wstring(reinterpret_cast<uintptr_t>(this));
  WNDCLASSEXW c{};c.cbSize=sizeof(c);c.hInstance=instance;c.lpfnWndProc=Impl::dispatch;c.lpszClassName=impl->className.c_str();
  impl->atom=RegisterClassExW(&c);
  if(!impl->atom)throw std::runtime_error("Cannot register UI task window");
  impl->window=CreateWindowExW(0,c.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,instance,nullptr);
  if(!impl->window) {UnregisterClassW(c.lpszClassName,instance);throw std::runtime_error("Cannot create UI task window");}
  SetWindowLongPtrW(impl->window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(impl.get()));
}
Win32TaskExecutor::~Win32TaskExecutor() noexcept {
  waitAllTasksExecuted();
  impl->workers.clear(); // joins all workers before any module code can unload
  DestroyWindow(impl->window);UnregisterClassW(impl->className.c_str(),impl->instance);
}
const Tasks::Queue& Win32TaskExecutor::getMainQueue() const {return impl->main;}
const Tasks::Queue& Win32TaskExecutor::getBackgroundQueue() const {return impl->background;}
Tasks::Queue Win32TaskExecutor::makeSerialQueue(const char*) const {
  std::lock_guard lock(impl->mutex);auto id=impl->next++;impl->workers.emplace(id,std::make_shared<Worker>());return {id};
}
void Win32TaskExecutor::releaseSerialQueue(const Tasks::Queue& queue) const {
  if(queue.identifier<2)return;
  auto worker=impl->worker(queue.identifier);if(!worker)return;
  while(worker->pending)impl->pump();
  {std::lock_guard lock(impl->mutex);impl->workers.erase(queue.identifier);}
  worker.reset();
}
void Win32TaskExecutor::schedule(const Tasks::Queue& queue,Tasks::Task&& task) const {
  if(queue==impl->main) {
    auto owned=std::make_unique<Tasks::Task>(std::move(task));++impl->mainPending;
    if(!PostMessageW(impl->window,Impl::message,reinterpret_cast<WPARAM>(owned.get()),0)) {
      --impl->mainPending;throw std::runtime_error("Cannot post UI task");
    }
    owned.release();
  } else if(auto worker=impl->worker(queue.identifier))worker->schedule(std::move(task));
}
void Win32TaskExecutor::waitAllTasksExecuted(const Tasks::Queue& queue) const {
  if(queue==impl->main) {while(impl->mainPending)impl->pump();}
  else if(auto worker=impl->worker(queue.identifier)) {while(worker->pending)impl->pump();}
}
void Win32TaskExecutor::waitAllTasksExecuted() const {
  for(;;) {
    bool pending=impl->mainPending!=0;
    {std::lock_guard lock(impl->mutex);for(const auto& entry:impl->workers)pending=pending || entry.second->pending!=0;}
    if(!pending)return;impl->pump();
  }
}
}
