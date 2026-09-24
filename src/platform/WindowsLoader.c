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
/* On a failed load, report the exact unresolved import without calling any
 * plugin entry point. This only runs during module initialization failure. */
static void diagnoseImports(const wchar_t* path, unsigned depth) {
  if(depth>8)return;
  HMODULE image=LoadLibraryExW(path,NULL,DONT_RESOLVE_DLL_REFERENCES);
  if(!image)return;
  BYTE* base=(BYTE*)image;
  IMAGE_DOS_HEADER* dos=(IMAGE_DOS_HEADER*)base;
  IMAGE_NT_HEADERS* nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);
  DWORD imports=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  if(imports)for(IMAGE_IMPORT_DESCRIPTOR* item=(IMAGE_IMPORT_DESCRIPTOR*)(base+imports);item->Name;++item) {
    const char* name=(const char*)(base+item->Name);
    wchar_t dependency[32768];
    wcscpy(dependency,path);
    wchar_t* slash=wcsrchr(dependency,L'\\');
    if(!slash)continue;
    MultiByteToWideChar(CP_ACP,0,name,-1,slash+1,(int)(32768-(slash+1-dependency)));
    HMODULE module=LoadLibraryExW(dependency,NULL,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module)module=LoadLibraryExA(name,NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module) {
      fprintf(stderr,"OrganVST dependency failed: %s (%lu)\n",name,(unsigned long)GetLastError());
      diagnoseImports(dependency,depth+1);
      continue;
    }
    if(item->OriginalFirstThunk)for(IMAGE_THUNK_DATA* thunk=(IMAGE_THUNK_DATA*)(base+item->OriginalFirstThunk);thunk->u1.AddressOfData;++thunk) {
      if(IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal))continue;
      const char* symbol=(const char*)((IMAGE_IMPORT_BY_NAME*)(base+thunk->u1.AddressOfData))->Name;
      if(!GetProcAddress(module,symbol))fprintf(stderr,"OrganVST missing import: %s!%s\n",name,symbol);
    }
    FreeLibrary(module);
  }
  FreeLibrary(image);
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
    diagnoseImports(path,0);
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
