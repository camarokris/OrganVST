// SPDX-License-Identifier: GPL-2.0-or-later
#include "Plugin.h"
#include "public.sdk/source/main/pluginfactory.h"
using namespace Steinberg;
using namespace Steinberg::Vst;
BEGIN_FACTORY_DEF("OrganVST contributors", "", "")
DEF_CLASS2(INLINE_UID_FROM_FUID(organvst::processorId),PClassInfo::kManyInstances,
  kVstAudioEffectClass,"OrganVST",0,"Instrument|Sampler","0.1.0",kVstVersionString,organvst::Processor::create)
DEF_CLASS2(INLINE_UID_FROM_FUID(organvst::controllerId),PClassInfo::kManyInstances,
  kVstComponentControllerClass,"OrganVST Controller",0,"","0.1.0",kVstVersionString,organvst::Controller::create)
END_FACTORY
