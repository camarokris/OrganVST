// SPDX-License-Identifier: GPL-2.0-or-later
// Device discovery is deliberately absent: the DAW owns all hardware.
#include "sound/ports/GOSoundPortFactory.h"
#include "midi/ports/GOMidiPortFactory.h"
#include "GOEvent.h"
#include <wx/string.h>
#include <stdexcept>

const std::vector<wxString>& GOSoundPortFactory::GetPortNames() const { return c_NoApis; }
const std::vector<wxString>& GOSoundPortFactory::GetPortApiNames(const wxString&) const { return c_NoApis; }
GOSoundPortFactory& GOSoundPortFactory::getInstance() { static GOSoundPortFactory f; return f; }
std::vector<GOSoundDevInfo> GOSoundPortFactory::getDeviceList(const GOPortsConfig&) { return {}; }
GOSoundPort* GOSoundPortFactory::create(const GOPortsConfig&, GOSoundCallbackConnector&, GODeviceNamePattern&) { return nullptr; }
void GOSoundPortFactory::terminate() {}
const std::vector<wxString>& GOMidiPortFactory::GetPortNames() const { return c_NoApis; }
const std::vector<wxString>& GOMidiPortFactory::GetPortApiNames(const wxString&) const { return c_NoApis; }
GOMidiPortFactory& GOMidiPortFactory::getInstance() { static GOMidiPortFactory f; return f; }
void GOMidiPortFactory::addMissingInDevices(GOMidiSystem*, const GOPortsConfig&, ptr_vector<GOMidiPort>&) {}
void GOMidiPortFactory::addMissingOutDevices(GOMidiSystem*, const GOPortsConfig&, ptr_vector<GOMidiPort>&) {}
void GOMidiPortFactory::terminate() {}

// Turn legacy load-error dialogs into recoverable controller load failures.
void GOMessageBox(const wxString& text, const wxString, long, wxWindow*) {
  throw std::runtime_error(text.ToStdString());
}
void GOAskRenameFile(const wxString&, const wxString, const wxString&) {
  throw std::runtime_error("Standalone recording is not available in the plugin");
}
DEFINE_LOCAL_EVENT_TYPE(wxEVT_SETVALUE)
bool is_to_import_to_this_organ(const wxString& current, const wxString&,
                               const wxString&, const wxString& source) {
  if (current != source) throw std::runtime_error("Combination belongs to a different organ");
  return true;
}
