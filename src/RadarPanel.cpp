/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  Navico BR24 Radar Plugin
 * Authors:  David Register
 *           Dave Cowell
 *           Kees Verruijt
 *           Douwe Fokkema
 *           Sean D'Epagnier
 ***************************************************************************
 *   Copyright (C) 2010 by David S. Register              bdbcat@yahoo.com *
 *   Copyright (C) 2012-2013 by Dave Cowell                                *
 *   Copyright (C) 2012-2016 by Kees Verruijt         canboat@verruijt.net *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

#include "RadarPanel.h"
#include "RadarCanvas.h"

PLUGIN_BEGIN_NAMESPACE

enum {  // process ID's
  ID_CONFIRM,
  ID_CLOSE
};

RadarPanel::RadarPanel(br24radar_pi* pi, RadarInfo* ri, wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL, _("RADAR")) {
  m_parent = parent;
  m_pi = pi;
  m_ri = ri;
}

bool RadarPanel::Create() {
  m_aui_mgr = GetFrameAuiManager();

  m_aui_name = wxString::Format(wxT("BR24radar_pi-%d"), m_ri->radar);
  wxAuiPaneInfo pane = wxAuiPaneInfo()
                           .Name(m_aui_name)
                           .Caption(m_ri->name)
                           .CaptionVisible(true)  // Show caption even when docked
                           .TopDockable(false)
                           .BottomDockable(false)
                           .RightDockable(true)
                           .LeftDockable(true)
                           .CloseButton(true)
                           .Gripper(false);

  m_ri->radar_canvas =
      new RadarCanvas(m_pi, m_ri, this, wxSize(512, 512));  // m_pi->m_dialogLocation[DL_RADARWINDOW + radar].size);
  if (!m_ri->radar_canvas) {
    wxLogMessage(wxT("BR24radar_pi %s: Unable to create RadarCanvas"), m_ri->name.c_str());
    return false;
  }

  wxBoxSizer* Sizer = new wxBoxSizer(wxHORIZONTAL);
  Sizer->Add(m_ri->radar_canvas, 0, wxEXPAND | wxALL, 0);
  SetSizer(Sizer);

  DimeWindow(this);
  Fit();
  Layout();
  // SetMinSize(GetBestSize());
  Refresh();

  wxSize screen_size = ::wxGetDisplaySize();

  m_best_size.x = screen_size.x / 2;
  m_best_size.y = screen_size.y / 2;

  pane.MinSize(wxSize(256, 256));
  pane.BestSize(m_best_size);
  pane.FloatingSize(wxSize(512, 512));
  pane.Float();
  pane.dock_proportion = 100000;  // Secret sauce to get panels to use entire bar
  pane.Hide();

  m_aui_mgr->AddPane(this, pane);
  m_aui_mgr->Update();
  m_aui_mgr->Connect(wxEVT_AUI_PANE_CLOSE, wxAuiManagerEventHandler(RadarPanel::close), NULL, this);

  m_dock_size = 0;

  if (m_pi->m_perspective[m_ri->radar].length()) {
    // Do this first and it doesn't work if the pane starts docked.
    wxLogMessage(wxT("BR24radar_pi: Restoring panel %s to AUI control manager: %s"), m_aui_name.c_str(),
                 m_pi->m_perspective[m_ri->radar].c_str());
    m_aui_mgr->LoadPaneInfo(m_pi->m_perspective[m_ri->radar], pane);
    m_aui_mgr->Update();
  } else {
    wxLogMessage(wxT("BR24radar_pi: Added panel %s to AUI control manager"), m_aui_name.c_str());
  }

  return true;
}

RadarPanel::~RadarPanel() {
  m_pi->m_perspective[m_ri->radar] = m_aui_mgr->SavePaneInfo(m_aui_mgr->GetPane(this));
  if (m_ri->radar_canvas) {
    delete m_ri->radar_canvas;
    m_ri->radar_canvas = 0;
  }
  m_aui_mgr->DetachPane(this);
  m_aui_mgr->Update();
  wxLogMessage(wxT("BR24radar_pi: %s panel removed"), m_ri->name.c_str());
}

void RadarPanel::SetCaption(wxString name) { m_aui_mgr->GetPane(this).Caption(name); }

void RadarPanel::close(wxAuiManagerEvent& event) { event.Skip(); }

void RadarPanel::ShowFrame(bool visible) {
  wxLogMessage(wxT("BR24radar_pi %s: set visible %d"), m_ri->name.c_str(), visible);

  wxAuiPaneInfo& pane = m_aui_mgr->GetPane(this);
  if (!visible) {
    m_dock_size = 0;
    wxLogMessage(wxT("BR24radar_pi: %s: size = %d,%d"), m_ri->name.c_str(), GetSize().x, GetSize().y);
    if (pane.IsDocked()) {
      m_dock = wxString::Format(wxT("|dock_size(%d,%d,%d)="), pane.dock_direction, pane.dock_layer, pane.dock_row);
      wxString perspective = m_aui_mgr->SavePerspective();

      int p = perspective.Find(m_dock);
      if (p != wxNOT_FOUND) {
        perspective = perspective.Mid(p + m_dock.length());
        perspective = perspective.BeforeFirst(wxT('|'));
        m_dock_size = wxAtoi(perspective);
        wxLogMessage(wxT("BR24radar_pi: %s: replaced=%s, saved dock_size = %d"), m_ri->name.c_str(), perspective.c_str(),
                     m_dock_size);
      }
    }
  } else {
    pane.Position(m_ri->radar);
  }

  pane.Show(visible);
  m_aui_mgr->Update();

  if (visible && (m_dock_size > 0)) {
    wxString perspective = m_aui_mgr->SavePerspective();
    wxLogMessage(wxT("BR24radar_pi: %s: old perspective %s"), m_ri->name.c_str(), perspective.c_str());

    int p = perspective.Find(m_dock);
    if (p != wxNOT_FOUND) {
      wxString newPerspective = perspective.Left(p);
      newPerspective << m_dock;
      newPerspective << m_dock_size;
      perspective = perspective.Mid(p + m_dock.length());
      newPerspective << wxT("|");
      newPerspective << perspective.AfterFirst(wxT('|'));

      m_aui_mgr->LoadPerspective(newPerspective);
      if (m_pi->m_settings.verbose >= 1) {
        wxLogMessage(wxT("BR24radar_pi: %s: new perspective %s"), m_ri->name.c_str(), newPerspective.c_str());
      }
    }
  }
}

bool RadarPanel::IsShown() { return m_aui_mgr->GetPane(this).IsShown(); }

wxPoint RadarPanel::GetPos() {
  if (m_aui_mgr->GetPane(this).IsFloating()) {
    return GetParent()->GetScreenPosition();
  }
  return GetScreenPosition();
}

PLUGIN_END_NAMESPACE