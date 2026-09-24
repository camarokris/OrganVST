// SPDX-License-Identifier: GPL-2.0-or-later
#include "vstgui/lib/platform/win32/win32taskexecutor.h"
#include <atomic>
#include <stdexcept>
int main() {
  VSTGUI::Win32TaskExecutor executor(GetModuleHandleW(nullptr));
  std::atomic<int> count{0};std::atomic<bool> ordered{true};
  const auto queue=executor.makeSerialQueue("test");
  for(int i=0;i<100;++i)executor.schedule(queue,[&,i]{if(count.fetch_add(1)!=i)ordered=false;});
  executor.releaseSerialQueue(queue);
  if(count!=100 || !ordered)return 1;
  executor.schedule(executor.getBackgroundQueue(),[&]{executor.schedule(executor.getMainQueue(),[&]{++count;});});
  executor.waitAllTasksExecuted();
  if(count!=101)return 2;
  {VSTGUI::Win32TaskExecutor other(GetModuleHandleW(nullptr));
   other.schedule(other.getBackgroundQueue(),[&]{++count;});}
  return count==102?0:3;
}
