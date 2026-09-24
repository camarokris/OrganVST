// SPDX-License-Identifier: GPL-2.0-or-later
#include "Plugin.h"
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/cvstguitimer.h"
#include <sstream>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace organvst {
namespace {
class View final : public VSTGUI::CView {
public:
  View(Controller& c):CView(VSTGUI::CRect(0,0,1000,700)),controller(c){}
  void draw(VSTGUI::CDrawContext* context) override {
    using namespace VSTGUI;
    context->setFillColor(CColor(23,27,32));context->drawRect(getViewSize(),kDrawFilled);
    context->setFont(kNormalFontVeryBig);context->setFontColor(CColor(234,220,181));
    context->drawString("OrganVST",CRect(24,18,850,60),kLeftText);
    context->setFont(kNormalFont);context->setFontColor(kWhiteCColor);
    context->drawString(controller.status.c_str(),CRect(24,62,970,90),kLeftText);
    context->setFillColor(CColor(64,78,87));context->drawRect(CRect(830,18,976,54),kDrawFilled);
    context->drawString("Load organ…",CRect(830,18,976,54));
    context->drawString("Stops & Couplers",CRect(24,104,650,132),kLeftText);
    std::istringstream stream(controller.metadata);std::string name;unsigned index=0;
    while(std::getline(stream,name)) {
      if(index>=page*36 && index<(page+1)*36 && index<128) {
        unsigned cell=index-page*36,col=cell%3,row=cell/3;
        CRect rect(24+col*320,146+row*40,332+col*320,180+row*40);
        context->setFillColor(controller.getParamNormalized(stopBase+index)>=.5?CColor(140,110,54):CColor(42,48,55));
        context->drawRect(rect,kDrawFilled);context->drawString(name.c_str(),rect);
      }
      ++index;
    }
    context->drawString("Previous",CRect(24,642,140,680));
    context->drawString("Next",CRect(156,642,272,680));
    context->drawString("Export diagnostics",CRect(790,642,980,680));
    setDirty(false);
  }
  VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& p,const VSTGUI::CButtonState&) override {
    using namespace VSTGUI;
    if(CRect(830,18,976,54).pointInside(p)) {
      auto selector=VSTGUI::owned(CNewFileSelector::create(getFrame()));
      selector->setTitle("Load an organ definition");
      selector->addFileExtension(CFileExtension("Organ definition","organ"));
      selector->addFileExtension(CFileExtension("Organ definition","odf"));
      auto* target=&controller;target->addRef();
      selector->run([target](CNewFileSelector* selected) {
        if(selected->getNumSelectedFiles())target->load(selected->getSelectedFile(0));
        target->release();
      });
    } else if(p.y>=642 && p.x>=790) {
      auto selector=VSTGUI::owned(CNewFileSelector::create(getFrame(),CNewFileSelector::kSelectSaveFile));
      selector->setTitle("Export diagnostics");selector->setDefaultSaveName("OrganVST-diagnostics.zip");
      auto* target=&controller;target->addRef();
      selector->run([target](CNewFileSelector* selected){if(selected->getNumSelectedFiles())target->exportDiagnostics(selected->getSelectedFile(0));target->release();});
    } else if(p.y>=642) {
      if(p.x<140 && page) --page;
      else if(p.x>=156 && p.x<272 && page<3)++page;
    } else if(p.x>=24 && p.x<984 && p.y>=146 && p.y<626) {
      unsigned col=unsigned(p.x-24)/320,row=unsigned(p.y-146)/40,index=page*36+row*3+col;
      if(index<128) {
        auto id=stopBase+index;auto value=controller.getParamNormalized(id)>=.5?0.0:1.0;
        controller.beginEdit(id);controller.setParamNormalized(id,value);controller.performEdit(id,value);controller.endEdit(id);
      }
    }
    invalid();return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
  }
private:
  Controller& controller;unsigned page=0;
};
class Editor final : public VSTGUIEditor {
public:
  explicit Editor(Controller* c):VSTGUIEditor(c),controller(*c){rect=ViewRect(0,0,1000,700);}
  bool PLUGIN_API open(void* parent,const VSTGUI::PlatformType& type) override {
    frame=new VSTGUI::CFrame(VSTGUI::CRect(0,0,1000,700),this);
    view=new View(controller);frame->addView(view);
    if(!frame->open(parent,type)){frame->forget();frame=nullptr;view=nullptr;return false;}
    timer=VSTGUI::makeOwned<VSTGUI::CVSTGUITimer>([this](auto*){controller.poll();if(view)view->invalid();},200);
    return true;
  }
  void PLUGIN_API close() override {
    timer=nullptr;view=nullptr;if(frame){frame->close();frame->forget();frame=nullptr;}
  }
private:
  Controller& controller;View* view=nullptr;VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer;
};
}
tresult PLUGIN_API Controller::initialize(FUnknown* context) {
  auto result=EditController::initialize(context);if(result!=kResultOk)return result;
  parameters.addParameter(STR16("Master gain"),nullptr,0,.5,ParameterInfo::kCanAutomate,gainId);
  for(unsigned i=0;i<128;++i) {
    String128 title{};auto s=std::string("Organ control ")+std::to_string(i+1);
    for(unsigned j=0;j<s.size()&&j<127;++j)title[j]=s[j];
    parameters.addParameter(title,nullptr,1,0,ParameterInfo::kCanAutomate,stopBase+i);
  }
  return kResultOk;
}
IPlugView* PLUGIN_API Controller::createView(FIDString name) { return std::strcmp(name,ViewType::kEditor)==0?new Editor(this):nullptr; }
void Controller::load(const std::string& path) {
  auto m=owned(allocateMessage());if(!m)return;m->setMessageID("load");
  m->getAttributes()->setBinary("path",path.data(),uint32(path.size()));sendMessage(m);
}
void Controller::poll() { auto m=owned(allocateMessage());if(m){m->setMessageID("poll");sendMessage(m);} }
void Controller::exportDiagnostics(const std::string& path) {
  auto m=owned(allocateMessage());if(!m)return;m->setMessageID("diagnostics");
  m->getAttributes()->setBinary("path",path.data(),uint32(path.size()));sendMessage(m);
}
tresult PLUGIN_API Controller::notify(IMessage* m) {
  if(m && std::strcmp(m->getMessageID(),"status")==0) {
    const void* bytes=nullptr;uint32 size=0;
    if(m->getAttributes()->getBinary("status",bytes,size)==kResultOk)status.assign(static_cast<const char*>(bytes),size);
    if(m->getAttributes()->getBinary("controls",bytes,size)==kResultOk)metadata.assign(static_cast<const char*>(bytes),size);
    return kResultOk;
  }
  return EditController::notify(m);
}
}
