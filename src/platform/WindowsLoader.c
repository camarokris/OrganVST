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
  /* wxWidgets imports Common Controls v6 APIs. A DLL cannot rely on the
   * host executable opting into that assembly (the SDK tools do not). */
  wchar_t loaderPath[32768];
  GetModuleFileNameW(ownModule,loaderPath,32768);
  ACTCTXW context={0};
  context.cbSize=sizeof(context);
  context.dwFlags=ACTCTX_FLAG_RESOURCE_NAME_VALID;
  context.lpSource=loaderPath;
  context.lpResourceName=MAKEINTRESOURCEW(2);
  HANDLE activation=CreateActCtxW(&context);
  ULONG_PTR cookie=0;
  if(activation==INVALID_HANDLE_VALUE || !ActivateActCtx(activation,&cookie)) {
    fprintf(stderr,"OrganVST: cannot activate Common Controls v6 (%lu)\n",(unsigned long)GetLastError());
    if(activation!=INVALID_HANDLE_VALUE)ReleaseActCtx(activation);
    return false;
  }
  engine=LoadLibraryExW(path,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  DWORD loadError=GetLastError();
  DeactivateActCtx(0,cookie);
  ReleaseActCtx(activation);
  if(!engine) {
    DWORD error=loadError;
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
