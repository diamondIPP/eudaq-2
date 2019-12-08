#include "eudaq/FileNamer.hh"
#include "eudaq/FileWriter.hh"
#include "eudaq/Configuration.hh"
#include "StandardEvent.hh"
#include "StdEventConverter.hh"
#include "TTreeFileWriter.hh"
#include <utility>

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

  TTreeFileWriter::TTreeFileWriter(string out, const string & in) : m_filepattern(move(out)), m_config(nullptr), m_n_planes(0), m_n_cms_pixels(0), m_init_vectors(false), b_time_stamp_begin(0),
                                                                    b_time_stamp_end(0) {

    m_run_n = stoi(in.substr(in.rfind('/') + 4, in.rfind('_') - in.rfind('/') - 4));
	  string foutput(FileNamer(m_filepattern).Set('R', m_run_n));
    gROOT->ProcessLine("gErrorIgnoreLevel = 5001;");
    gROOT->ProcessLine("#include <vector>");
    gInterpreter->GenerateDictionary("vector<vector<Float_t> >;vector<vector<UShort_t> >");
    EUDAQ_INFO("Preparing the outputfile: " + foutput);
    m_tfile = new TFile(foutput.c_str(), "RECREATE");
    m_ttree = new TTree("tree","Converted from .raw");
    SetBranchAddresses();
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
        InitVectors();
      }
    }
    FillVectors(stdev);
    if (m_init_vectors){
      m_ttree->Fill();
      ClearVectors();
    }
  }

  void TTreeFileWriter::SetBranchAddresses() {
    
    m_ttree->Branch("event_number", &b_event_nr, "event_number/i");
    m_ttree->Branch("time_begin", &b_time_stamp_begin, "time_begin/D");
    m_ttree->Branch("time_end", &b_time_stamp_end, "time_end/D");
    m_ttree->Branch("n_hits", &b_n_hits);
    m_ttree->Branch("trigger_phase", &b_trigger_phase);
    m_ttree->Branch("trigger_phase", &b_trigger_count);

    // telescope
    m_ttree->Branch("column", &b_column);
    m_ttree->Branch("row", &b_row);
    m_ttree->Branch("adc", &b_adc);
    m_ttree->Branch("charge", &b_charge);
//
//    // tu
//    if (hasTU){
//      m_ttree->Branch("beam_current", &f_beam_current, "beam_current/s");
//      m_ttree->Branch("rate", &v_scaler);
//    }
  }

  void TTreeFileWriter::ClearVectors() {

    for (uint8_t i(0); i < m_n_planes; i++){
      b_column->at(i).clear();
      b_row->at(i).clear();
      b_adc->at(i).clear();
      b_charge->at(i).clear();
    }
  }

  void TTreeFileWriter::InitVectors() {

    b_n_hits.resize(m_n_planes);
    b_column = new vector<vector<uint16_t>>(m_n_planes);
    b_row = new vector<vector<uint16_t>>(m_n_planes);
    b_adc = new vector<vector<uint16_t>>(m_n_planes);
    b_charge = new vector<vector<float>>(m_n_planes);
    m_init_vectors = true;
  }

  void TTreeFileWriter::FillVectors(const StandardEventSPC & stdev) {

    b_trigger_phase = stdev->GetTriggerPhases();
    b_trigger_count = stdev->GetTriggerCounts();
    for (size_t i(0); i < stdev->NumPlanes(); i++){
      const auto & plane = stdev->GetPlane(i);
      b_n_hits.at(i) = plane.HitPixels();
      for (size_t i_frame(0); i_frame < plane.NumFrames(); i_frame++)
        for (size_t i_hit(0); i_hit < plane.HitPixels(i_frame); i_hit++){
          b_column->at(i).push_back(plane.GetX(i_hit, i_frame));
          b_row->at(i).push_back(plane.GetY(i_hit, i_frame));
          b_adc->at(i).push_back(plane.GetPixel(i_hit, i_frame));
          b_charge->at(i).push_back(plane.GetCharge(i_hit, i_frame));
        }
    }
  }

  uint8_t TTreeFileWriter::FindNCMSPixels(const EventSPC & ev) {

    uint8_t n(0);
    for (const auto & sev: ev->GetSubEvents())
      if (sev->GetDescription().find("CMS") != string::npos)
        n++;
    return n;
  }
}
