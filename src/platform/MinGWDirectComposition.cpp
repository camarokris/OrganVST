// SPDX-License-Identifier: GPL-2.0-or-later
// MinGW headers lack IDCompositionVisual3. Returning no composition factory
// selects VSTGUI's existing HWND/Direct2D renderer, including its normal redraws.
#include "vstgui/lib/platform/win32/win32directcomposition.h"
namespace VSTGUI::DirectComposition {
struct Factory::Impl {};
Factory::Factory() = default;
Factory::~Factory() noexcept = default;
std::unique_ptr<Factory> Factory::create(IUnknown*) { return nullptr; }
bool Factory::enableVisualizeRedrawAreas(bool) { return false; }
bool Factory::isVisualRedrawAreasEnabled() const { return false; }
VisualPtr Factory::createVisualForHWND(HWND) { return {}; }
VisualPtr Factory::createChildVisual(const VisualPtr&,uint32_t,uint32_t) { return {}; }
bool Factory::removeVisual(const VisualPtr&) { return false; }
ID2D1Device* Factory::getDevice() const { return nullptr; }
}
