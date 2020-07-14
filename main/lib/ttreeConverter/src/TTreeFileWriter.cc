#include "eudaq/FileNamer.hh"
#include "eudaq/FileWriter.hh"
#include "eudaq/Configuration.hh"
#include "StandardEvent.hh"
#include "StdEventConverter.hh"
#include "TTreeFileWriter.hh"
#include <utility>
#include "Utils.hh"

#include "TFile.h"
#include "TTree.h"
#include "TInterpreter.h"
#include "TROOT.h"

using namespace std;

namespace eudaq {

  class TTreeFileWriter;
  namespace{
    auto dummy01 = Factory<FileWriter>::Register<TTreeFileWriter, std::string&, std::string&>(cstr2hash("root"));
    auto dummy11 = Factory<FileWriter>::Register<TTreeFileWriter, std::string&&, std::string&&>(cstr2hash("root"));
  }

  TTreeFileWriter::TTreeFileWriter(string out, const string & in) : m_log_type("TreeWriter"), m_filepattern(move(out)), m_config(nullptr), m_n_planes(0),
                                                                    m_n_telescope_planes(0), m_n_cms_pixels(0), m_init_vectors(false), m_max_hits(500),
                                                                    b_event_nr(0), b_time_stamp_begin(0),  b_time_stamp_end(0), b_frame_number(-1), b_trigger_time(0),
                                                                    b_invalid(false) {

    m_run_n = stoi(in.substr(in.rfind('/') + 4, in.rfind('_') - in.rfind('/') - 4));
	  string foutput(FileNamer(m_filepattern).Set('R', m_run_n));
    gROOT->ProcessLine("gErrorIgnoreLevel = 5001;");
    gROOT->ProcessLine("#include <vector>");
    EUDAQ_INFO_OUT("Preparing the outputfile: " + foutput, m_log_type);
    m_tfile = new TFile(foutput.c_str(), "RECREATE");
    m_event_tree = new TTree("Event", "Event Information");
    m_plane_indices = {};
  }

  TTreeFileWriter::~TTreeFileWriter () {

    m_tfile->cd();
    m_tfile->Write();
    m_tfile->Close();
    delete m_tfile;
  }
  
  void TTreeFileWriter::WriteEvent(EventSPC cev) {

    if(!m_tfile)
      EUDAQ_THROW("TTreeFileWriter: Attempt to write unopened file");
    auto ev = const_pointer_cast<Event>(cev);
    b_event_nr = ev->GetEventN();
    for (const auto & sev: ev->GetSubEvents()) {
      if (sev->GetDescription() == "TluRawDataEvent") {
        b_time_stamp_begin = sev->GetTimestampBegin();
        b_time_stamp_end = sev->GetTimestampEnd();
      }
    }
    auto stdev = dynamic_pointer_cast<StandardEvent>(ev);
    if(!stdev){
      stdev = StandardEvent::MakeShared();
      StdEventConverter::Convert(ev, stdev, nullptr); //no conf
    }
    if (not m_init_vectors) {
      if (stdev->NumPlanes() > 0){
        m_n_planes = stdev->NumPlanes();
        m_n_cms_pixels = FindNCMSPixels(ev);
        m_n_telescope_planes = m_n_planes - m_n_cms_pixels;
        InitPlaneIndices(ev);
        InitTrees();
        InitVectors();
        SetBranches();
      }
    }
    if (stdev->NumPlanes() == m_n_planes){  // only write the event if we have the correct number of planes...
      if (m_init_vectors and stdev->NumPlanes() == m_n_planes) {  // only write the event if we have the correct number of planes...
        FillVectors(stdev);
        m_event_tree->Fill(); }
    } else {
      string msg = TString::Format("Incorrect number of planes (%u) in event %d", int(stdev->NumPlanes()), b_event_nr).Data();
      EUDAQ_WARN_OUT(msg, m_log_type); }
  }

  void TTreeFileWriter::SetBranches() {

    for (uint8_t i_plane(0); i_plane < m_n_planes; i_plane++){
      m_plane_trees.at(i_plane)->Branch("NHits", &b_n_hits.at(i_plane), "NHits/I");
      m_plane_trees.at(i_plane)->Branch("PixX", b_column.at(i_plane), "PixX[NHits]/I");
      m_plane_trees.at(i_plane)->Branch("PixY", b_row.at(i_plane), "PixY[NHits]/I");
      m_plane_trees.at(i_plane)->Branch("Value", b_adc.at(i_plane), "Value[NHits]/I");
      m_plane_trees.at(i_plane)->Branch("Timing", b_timing.at(i_plane), "Timing[NHits]/I");
      m_plane_trees.at(i_plane)->Branch("HitInCluster", b_hit_in_cluster.at(i_plane), "HitInCluster[NHits]/I");
      if (i_plane >= m_n_telescope_planes){
        m_plane_trees.at(i_plane)->Branch("TriggerPhase", &b_trigger_phase.at(i_plane - m_n_telescope_planes), "TriggerPhase/s");
        m_plane_trees.at(i_plane)->Branch("TriggerCount", &b_trigger_count.at(i_plane - m_n_telescope_planes), "TriggerCount/s");
      }
    }
    m_event_tree->Branch("TimeStamp", &b_time_stamp_begin, "TimeStamp/l");
    m_event_tree->Branch("FrameNumber", &b_frame_number, "FrameNumber/l");
    m_event_tree->Branch("TriggerTime", &b_time_stamp_begin, "TriggerTime/l");
    m_event_tree->Branch("Invalid", &b_invalid, "Invalid/O");
    m_event_tree->Branch("TimeEnd", &b_time_stamp_end, "TimeEnd/l");
  }

  void TTreeFileWriter::InitVectors() {

    b_n_hits.resize(m_n_planes);
    b_column.resize(m_n_planes, new int[m_max_hits]);
    b_row.resize(m_n_planes, new int[m_max_hits]);
    b_adc.resize(m_n_planes, new int[m_max_hits]);
    b_charge.resize(m_n_planes, new double[m_max_hits]);
    b_timing.resize(m_n_planes, new int[m_max_hits]);
    b_hit_in_cluster.resize(m_n_planes, new int[m_max_hits]);
    b_trigger_phase.resize(m_n_cms_pixels, 0);
    b_trigger_count.resize(m_n_cms_pixels, 0);
    // these vectors are required for proteus
    for (uint8_t ipl(0); ipl < m_n_planes; ipl++) {
      for (unsigned i(0); i < m_max_hits; i++) {
        b_timing.at(ipl)[i] = 0;
        b_hit_in_cluster.at(ipl)[i] = 0; }
    }
    m_init_vectors = true;
  }

  void TTreeFileWriter::FillVectors(const StandardEventSPC & stdev) {

    for (uint8_t i(0); i < stdev->GetTriggerPhases().size(); i++ )
      b_trigger_phase.at(i) = stdev->GetTriggerPhases().at(i);
    for (uint8_t i(0); i < stdev->GetTriggerCounts().size(); i++ )
      b_trigger_count.at(i) = stdev->GetTriggerCounts().at(i);
    for (uint8_t i(0); i < m_n_planes; i++){
      const auto & plane = stdev->GetPlane(m_plane_indices.at(i));
      b_n_hits.at(i) = plane.HitPixels();
      uint16_t j(0);
      for (size_t i_frame(0); i_frame < plane.NumFrames(); i_frame++) {
        for (size_t i_hit(0); i_hit < plane.HitPixels(i_frame); i_hit++) {
          if (j == m_max_hits - 1)  // stop writing if the maximum size is reached
            continue;
          b_column.at(i)[j] = uint16_t(plane.GetX(i_hit, i_frame));
          b_row.at(i)[j] = uint16_t(plane.GetY(i_hit, i_frame));
          b_adc.at(i)[j] = uint16_t(plane.GetPixel(i_hit, i_frame));
//          b_charge.at(i)[j] = uint16_t(plane.GetCharge(i_hit, i_frame));
          j++;
        }
      }
      m_plane_trees.at(i)->Fill();
    }
    b_frame_number++;
  }

  uint8_t TTreeFileWriter::FindNCMSPixels(const EventSPC & ev) {

    uint8_t n(0);
    for (const auto & sev: ev->GetSubEvents()) {
      if (sev->GetDescription().find("CMS") != string::npos){
        n++; }
    }
    return n;
  }

  void TTreeFileWriter::InitTrees() {

    for (size_t i(0); i < m_n_planes; i++){
      m_tfile->cd();
      auto t_dir = m_tfile->mkdir(("Plane" + to_string(i)).c_str());
      t_dir->cd();
      m_plane_trees.push_back(new TTree("Hits", "Hit Information"));
    }
  }

  void TTreeFileWriter::InitPlaneIndices(const EventSPC & stdev) {
    /** REF and DUT plane may change the position in the data ... correct for that! */

    for (uint8_t i(0); i < m_n_telescope_planes; i++) {
      m_plane_indices.emplace_back(i); }
    for (const auto & sev: stdev->GetSubEvents()) {
      if (sev->GetDescription().find("REF") != string::npos){
        m_plane_indices.emplace_back(m_n_telescope_planes); }
      if (sev->GetDescription().find("DUT") != string::npos){
        m_plane_indices.emplace_back(m_n_telescope_planes + 1); }
    }
    vector<int> out(m_plane_indices.begin(), m_plane_indices.end());
    EUDAQ_INFO_OUT("Initialising plane order: " + to_string(out, " "), m_log_type);
  }

}
