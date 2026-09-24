/* SPDX-License-Identifier: GPL-2.0-or-later
 * Minimal host-facing VST3 loader: private runtime DLLs are resolved inside
 * this bundle, without changing the DAW's PATH or DLL directory globally.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <wchar.h>
static HMODULE ownModule, engine;
static SRWLOCK lock = SRWLOCK_INIT;
static unsigned references;
typedef bool (*Lifecycle)(void);
typedef void* (*Factory)(void);
static Lifecycle engineInit, engineExit;
static Factory engineFactory;
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
  (void)reserved;
  if(reason==DLL_PROCESS_ATTACH) {ownModule=module;DisableThreadLibraryCalls(module);}
  return TRUE;
}
static bool loadEngine(void) {
  if(engine)return true;
  wchar_t path[32768];
  DWORD length=GetModuleFileNameW(ownModule,path,32768);
  if(!length || length>=32768)return false;
  wchar_t* slash=wcsrchr(path,L'\\');
  if(!slash || (size_t)(slash-path)+32>=32768)return false;
  wcscpy(slash+1,L"OrganVST-engine.dll");
  engine=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if(!engine) {
    DWORD error=GetLastError();
    fprintf(stderr,"OrganVST: private engine load failed, Windows error %lu\n",(unsigned long)error);
    return false;
  }
  engineInit=(Lifecycle)GetProcAddress(engine,"InitDll");
  engineExit=(Lifecycle)GetProcAddress(engine,"ExitDll");
  engineFactory=(Factory)GetProcAddress(engine,"GetPluginFactory");
  if(!engineInit || !engineExit || !engineFactory) {
    FreeLibrary(engine);engine=NULL;return false;
  }
  return true;
}
__declspec(dllexport) bool InitDll(void) {
  AcquireSRWLockExclusive(&lock);
  bool ok=loadEngine() && engineInit();
  if(ok)++references;
  else if(engine && !references) {FreeLibrary(engine);engine=NULL;}
  ReleaseSRWLockExclusive(&lock);
  return ok;
}
__declspec(dllexport) void* GetPluginFactory(void) {
  AcquireSRWLockExclusive(&lock);
  void* factory=NULL;
  if(loadEngine()) {
    if(!references && engineInit())++references;
    if(references)factory=engineFactory();
  }
  ReleaseSRWLockExclusive(&lock);
  return factory;
}
__declspec(dllexport) bool ExitDll(void) {
  AcquireSRWLockExclusive(&lock);
  bool ok=true;
  if(engine && references) {
    ok=engineExit();
    if(!--references) {FreeLibrary(engine);engine=NULL;}
  }
  ReleaseSRWLockExclusive(&lock);
  return ok;
}
