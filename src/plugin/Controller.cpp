// SPDX-License-Identifier: GPL-2.0-or-later
#include "Plugin.h"
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/cvstguitimer.h"
#include <sstream>
#include <cstring>
#include <algorithm>

using namespace Steinberg;
using namespace Steinberg::Vst;
namespace organvst {
namespace {
class View final : public VSTGUI::CView {
public:
  View(Controller& c):CView(VSTGUI::CRect(0,0,1000,700)),controller(c){}
  std::vector<std::string> groups() const {
    std::vector<std::string> result;
    for(const auto& c:controller.catalog)if(std::find(result.begin(),result.end(),c.group)==result.end())result.push_back(c.group);
    return result;
  }
  std::vector<unsigned> visible() {
    auto all=groups();if(group>=all.size())group=0;
    std::vector<unsigned> result;
    for(unsigned i=0;i<controller.catalog.size();++i) {
      const auto& c=controller.catalog[i];
      if(!all.empty() && c.group==all[group] && (kind==0 || c.kind==kinds[kind]))result.push_back(i);
    }
    if(page*20>=result.size())page=0;
    return result;
  }
  const ControlDescriptor* selected() const {
    auto divisions=groups();
    for(const auto& c:controller.catalog)if(group<divisions.size() && c.group==divisions[group])return &c;
    return nullptr;
  }
  void updatePitch() {
    const auto* c=selected();if(!c || c->channel<0)return;
    if(pitchGroup!=c->key || pitchGeneration!=controller.generation) {
      pitchGroup=c->key;pitchGeneration=controller.generation;
      pitch=c->firstNote<=60 && c->lastNote>=60?60:c->firstNote;
    }
    pitch=std::clamp(pitch,c->firstNote,c->lastNote);
  }
  void draw(VSTGUI::CDrawContext* context) override {
    using namespace VSTGUI;
    auto button=[&](const CRect& r,const std::string& label,bool on=false) {
      context->setFillColor(on?CColor(140,110,54):CColor(42,48,55));context->drawRect(r,kDrawFilled);
      context->setFontColor(kWhiteCColor);context->drawString(label.c_str(),r);
    };
    context->setFillColor(CColor(23,27,32));context->drawRect(getViewSize(),kDrawFilled);
    context->setFont(kNormalFontVeryBig);context->setFontColor(CColor(234,220,181));
    context->drawString("OrganVST 0.3 — Division controls",CRect(24,18,660,58),kLeftText);
    context->setFont(kNormalFont);context->setFontColor(kWhiteCColor);
    context->drawString(controller.status.c_str(),CRect(24,64,976,96),kLeftText);
    button(CRect(830,18,976,54),"Load organ…");button(CRect(680,18,820,54),"Cancel load");
    for(unsigned i=0;i<5;++i)button(CRect(244+i*146,108,382+i*146,142),kinds[i],kind==i);
    auto indices=visible();auto divisions=groups();
    // Division selector also pages, so unusual packs do not lose auxiliary manuals.
    if(group<groupPage*11 || group>=(groupPage+1)*11)groupPage=group/11;
    for(unsigned i=groupPage*11;i<divisions.size() && i<(groupPage+1)*11;++i)
      button(CRect(24,156+(i%11)*38,226,190+(i%11)*38),divisions[i],group==i);
    if(divisions.size()>11){button(CRect(24,580,120,610),"Divisions <");button(CRect(126,580,226,610),"Divisions >");}
    for(unsigned cell=0;cell<20 && page*20+cell<indices.size();++cell) {
      unsigned index=indices[page*20+cell];const auto& c=controller.catalog[index];
      CRect rect(244+(cell%2)*366,156+(cell/2)*42,600+(cell%2)*366,194+(cell/2)*42);
      button(rect,c.kind+": "+c.name,index<controller.actual.size()&&controller.actual[index]);
    }
    updatePitch();const auto* selection=selected();
    const int channel=selection?selection->channel:-1;
    std::string routeName="MIDI channels";
    for(const auto& c:controller.catalog)if(c.channel==controller.inputDivision && c.channel>=0){routeName=c.group;break;}
    const auto hint=channel>=0?"Division: ch "+std::to_string(channel+1)+", MIDI notes "+std::to_string(selection->firstNote)+"–"+std::to_string(selection->lastNote)+" | Input route: "+routeName:"Global controls | Input route: "+routeName;
    context->drawString(hint.c_str(),CRect(244,580,976,606),kLeftText);
    button(CRect(244,610,452,640),"Play this division",channel>=0&&controller.inputDivision==channel);
    button(CRect(462,610,660,640),"Use MIDI channels",controller.inputDivision<0);
    const auto incoming=controller.lastInput>=0?"Received ch "+std::to_string(controller.lastInput/128+1)+" / note "+std::to_string(controller.lastInput%128):"No MIDI notes received";
    context->drawString(incoming.c_str(),CRect(674,610,976,640),kLeftText);
    button(CRect(24,646,124,682),"Previous");button(CRect(134,646,234,682),"Next");
    context->drawString(("Page "+std::to_string(page+1)+" / "+std::to_string(std::max(1u,unsigned((indices.size()+19)/20)))).c_str(),CRect(244,646,390,682));
    button(CRect(396,646,572,682),controller.ready?"Audition note "+std::to_string(pitch):"Waiting for host");
    button(CRect(582,646,626,682),"−");button(CRect(636,646,680,682),"+");
    button(CRect(790,646,976,682),"Export diagnostics");setDirty(false);
  }
  VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& p,const VSTGUI::CButtonState&) override {
    using namespace VSTGUI;
    if(CRect(680,18,820,54).pointInside(p))controller.cancelLoad();
    else if(CRect(830,18,976,54).pointInside(p)) {
      auto selector=VSTGUI::owned(CNewFileSelector::create(getFrame()));selector->setTitle("Load an organ definition");
      selector->addFileExtension(CFileExtension("Organ definition","organ"));selector->addFileExtension(CFileExtension("Organ definition","odf"));selector->addFileExtension(CFileExtension("Organ ZIP pack","zip"));
      auto* target=&controller;target->addRef();selector->run([target](CNewFileSelector* selected){if(selected->getNumSelectedFiles())target->load(selected->getSelectedFile(0));target->release();});
    } else if(CRect(790,646,976,682).pointInside(p)) {
      auto selector=VSTGUI::owned(CNewFileSelector::create(getFrame(),CNewFileSelector::kSelectSaveFile));selector->setTitle("Export diagnostics");selector->setDefaultSaveName("OrganVST-diagnostics.zip");
      auto* target=&controller;target->addRef();selector->run([target](CNewFileSelector* selected){if(selected->getNumSelectedFiles())target->exportDiagnostics(selected->getSelectedFile(0));target->release();});
    } else {
      auto indices=visible();auto divisions=groups();
      for(unsigned i=0;i<5;++i)if(CRect(244+i*146,108,382+i*146,142).pointInside(p)){kind=i;page=0;}
      for(unsigned i=groupPage*11;i<divisions.size() && i<(groupPage+1)*11;++i)
        if(CRect(24,156+(i%11)*38,226,190+(i%11)*38).pointInside(p)){group=i;page=0;}
      if(CRect(24,580,120,610).pointInside(p) && groupPage){--groupPage;group=groupPage*11;page=0;}
      if(CRect(126,580,226,610).pointInside(p) && (groupPage+1)*11<divisions.size()){++groupPage;group=groupPage*11;page=0;}
      if(CRect(24,646,124,682).pointInside(p) && page)--page;
      if(CRect(134,646,234,682).pointInside(p) && (page+1)*20<indices.size())++page;
      updatePitch();
      const auto* selection=selected();
      if(selection && selection->channel>=0) {
        if(CRect(244,610,452,640).pointInside(p))controller.route(selection->channel);
        if(CRect(582,646,626,682).pointInside(p) && pitch>selection->firstNote)--pitch;
        if(CRect(636,646,680,682).pointInside(p) && pitch<selection->lastNote)++pitch;
        if(CRect(396,646,572,682).pointInside(p))controller.audition(selection->channel,pitch);
      }
      if(CRect(462,610,660,640).pointInside(p))controller.route(-1);
      for(unsigned cell=0;cell<20 && page*20+cell<indices.size();++cell) {
        CRect r(244+(cell%2)*366,156+(cell/2)*42,600+(cell%2)*366,194+(cell/2)*42);
        if(r.pointInside(p)){unsigned i=indices[page*20+cell];controller.control(i,!(i<controller.actual.size()&&controller.actual[i]));break;}
      }
    }
    invalid();return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
  }
private:
  Controller& controller;unsigned group=0,groupPage=0,kind=0,page=0,pitch=60,pitchGeneration=0;
  std::string pitchGroup;
  const std::array<std::string,5> kinds{"All","Stop","Coupler","Switch","Tremulant"};
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
    timer=nullptr;view=nullptr;if(frame){frame->close();frame=nullptr;}
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
void Controller::cancelLoad() { auto m=owned(allocateMessage());if(m){m->setMessageID("cancel");sendMessage(m);} }
void Controller::poll() { auto m=owned(allocateMessage());if(m){m->setMessageID("poll");sendMessage(m);} }
void Controller::exportDiagnostics(const std::string& path) {
  auto m=owned(allocateMessage());if(!m)return;m->setMessageID("diagnostics");
  m->getAttributes()->setBinary("path",path.data(),uint32(path.size()));sendMessage(m);
}
void Controller::control(unsigned index,bool value) {
  if(!ready||index>=catalog.size())return;
  auto m=owned(allocateMessage());if(!m)return;m->setMessageID("control");
  m->getAttributes()->setInt("generation",generation);m->getAttributes()->setInt("index",index);m->getAttributes()->setInt("value",value?1:0);
  sendMessage(m);
  if(index<actual.size())actual[index]=value;
  if(index<128) {
    auto id=stopBase+index;beginEdit(id);setParamNormalized(id,value?1:0);performEdit(id,value?1:0);endEdit(id);
  }
}
void Controller::audition(int channel,int pitch) {
  if(!ready)return;
  auto m=owned(allocateMessage());if(!m)return;m->setMessageID("audition");
  m->getAttributes()->setInt("generation",generation);m->getAttributes()->setInt("index",channel*128+pitch);sendMessage(m);
}
void Controller::route(int channel) {
  if(!ready)return;
  auto m=owned(allocateMessage());if(!m)return;m->setMessageID("route");
  m->getAttributes()->setInt("generation",generation);m->getAttributes()->setInt("channel",channel);sendMessage(m);
}
tresult PLUGIN_API Controller::notify(IMessage* m) {
  if(m && std::strcmp(m->getMessageID(),"status")==0) {
    const void* bytes=nullptr;uint32 size=0;
    if(m->getAttributes()->getBinary("status",bytes,size)==kResultOk)status.assign(static_cast<const char*>(bytes),size);
    if(m->getAttributes()->getBinary("controls",bytes,size)==kResultOk) {
      std::string incoming(static_cast<const char*>(bytes),size);
      if(incoming!=metadata) {
        metadata=std::move(incoming);catalog.clear();
        std::istringstream lines(metadata);std::string line;
        while(std::getline(lines,line)) {
          std::istringstream fields(line);ControlDescriptor c;std::string channel,first,last;
          if(std::getline(fields,c.key,'\t')&&std::getline(fields,c.group,'\t')&&std::getline(fields,c.kind,'\t')&&std::getline(fields,channel,'\t')&&std::getline(fields,first,'\t')&&std::getline(fields,last,'\t')&&std::getline(fields,c.name)) {
            try{c.channel=std::stoi(channel);c.firstNote=std::stoul(first);c.lastNote=std::stoul(last);}catch(...){c.channel=-1;}
            catalog.push_back(std::move(c));
          }
        }
      }
    }
    int64 number=0;
    if(m->getAttributes()->getInt("generation",number)==kResultOk)generation=unsigned(number);
    if(m->getAttributes()->getInt("route",number)==kResultOk)inputDivision=int(number);
    if(m->getAttributes()->getInt("input",number)==kResultOk)lastInput=int(number);
    ready=m->getAttributes()->getInt("ready",number)==kResultOk&&number!=0;
    if(m->getAttributes()->getBinary("states",bytes,size)==kResultOk && size==catalog.size()) {
      actual.clear();auto* values=static_cast<const unsigned char*>(bytes);
      for(unsigned i=0;i<size;++i){actual.push_back(values[i]!=0);if(i<128)setParamNormalized(stopBase+i,values[i]?1:0);}
    }
    return kResultOk;
  }
  return EditController::notify(m);
}
}
