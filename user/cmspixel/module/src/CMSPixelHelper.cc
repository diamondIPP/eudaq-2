#include "CMSPixelHelper.hh"
#include <vector>
#include "dictionaries.h"
#include "constants.h"
#include "datasource_evt.h"
#include "Utils.hh"
#include <exception>
#include <fstream>
#include "Logger.hh"
#include "RawEvent.hh"

#if USE_LCIO
#  include "IMPL/LCEventImpl.h"
#  include "IMPL/TrackerRawDataImpl.h"
#  include "IMPL/TrackerDataImpl.h"
#  include "IMPL/LCCollectionVec.h"
#  include "IMPL/LCGenericObjectImpl.h"
#  include "UTIL/CellIDEncoder.h"
#  include "lcio.h"
#endif

#if USE_EUTELESCOPE
#  include "EUTELESCOPE.h"
#  include "EUTelSetupDescription.h"
#  include "EUTelEventImpl.h"
#  include "EUTelTrackerDataInterfacerImpl.h"
#  include "EUTelGenericSparsePixel.h"
#  include "EUTelRunHeaderImpl.h"

#include "EUTelCMSPixelDetector.h"
using eutelescope::EUTELESCOPE;
#endif

#include <TMath.h>
#include <TString.h>
#include <TF1.h>

using namespace std;
using namespace pxar;

namespace eudaq {

  CMSPixelHelper::CMSPixelHelper(const EventSPC & bore, const ConfigurationSPC & cnf): do_conversion(false), decodingOffset(25) {
    map<string, float> roc_calibrations = {{"psi46v2", 65}, {"psi46digv21respin", 47}, {"proc600", 47}};
    m_calibration_factor = roc_calibrations.at(bore->GetTag("ROCTYPE", "psi46v2"));
    f_fit_function = new TF1("fitfunc", "[3]*(TMath::Erf((x-[0])/[1])+[2])", -4096, 4096);
    m_event_type = "CMS" + bore->GetTag("EVENTTYPE", "REF");
    initialize(bore);
    cout << "CONFIG: " << (cnf != nullptr) << endl;
  }

  float CMSPixelHelper::calc_vcal(uint16_t roc, uint16_t col, uint16_t row, uint16_t adc) const {
    f_fit_function->SetParameters(&m_cal_parameters.at(roc).at(string(TString::Format("%02d%02d", col, row)))[0]);
    return f_fit_function->GetX(adc);
  }

  void CMSPixelHelper::initialize(const EventSPC & bore) {
    std::string roctype = bore->GetTag("ROCTYPE", "psi46v2");
    std::string tbmtype = bore->GetTag("TBMTYPE", "tbmemulator2");
    m_detector = bore->GetTag("DETECTOR", "");
    std::string pcbtype = bore->GetTag("PCBTYPE", "");
    m_rotated_pcb = pcbtype.find("-rot") != std::string::npos;

    // Get the number of planes:
    m_nplanes = bore->GetTag("PLANES", 1);
    m_roctype = DeviceDictionary::getInstance()->getDevCode(roctype);
    m_tbmtype = DeviceDictionary::getInstance()->getDevCode(tbmtype);
    if (m_roctype == 0x0)
      EUDAQ_ERROR("Roctype" + to_string((int) m_roctype) + " not propagated correctly to CMSPixelConverterPlugin");

    read_ph_calibration(bore); // REVERT IT BACK - IL
    // TODO: decoding Offset!

    std::cout << "CMSPixel Converter initialized with detector " << m_detector << ", Event Type " << m_event_type
              << ", TBM type " << tbmtype << " (" << static_cast<int>(m_tbmtype) << ")"
              << ", ROC type " << roctype << " (" << static_cast<int>(m_roctype) << ")" << std::endl;
  }

void CMSPixelHelper::read_ph_calibration(const EventSPC & bore) {

  cout << "TRY TO READ PH CALIBRATION DATA... " << endl;
  string roctype = bore->GetTag("ROCTYPE", "psi46v2");
  string file_name = bore->GetTag("DEVICEDIR", "");
  file_name = file_name.substr(0, file_name.rfind('/'));
//  filename_base = "/home/micha/software/data" + filename_base.substr(filename_base.rfind('/'));
  if (file_name.empty())
    return;
  file_name += ((roctype.find("dig") != string::npos) ? "/phCalibrationFitErr" : "/phCalibrationGErfFit") + bore->GetTag("TRIM", "");
  vector<string> i2cs = split(bore->GetTag("i2c", "0"), " ");
  vector<double> pars(4);
  uint16_t col, row;
  string dump;
  char trash[30];
  for (const auto & i2c : i2cs){
    FILE * fp;
    char * line = nullptr;
    size_t len = 0;
    file_name += "_C" + i2c + ".dat";
    cout << "Try to open ph calibration file in CMSPixelHelper: " << file_name << endl;
    fp = fopen(file_name.c_str(), "r");
    map<string, vector<double>> tmp_par_map;
    if (!fp){
      cout <<  "Could not open file: " << file_name << endl;
      return;
    } else {
      for (uint8_t i = 0; i < 3; i++) /** jump to fourth line */
        if (!getline(&line, &len, fp))
          return;
      while (fscanf(fp, "%lf %lf %lf %lf %s %hd %hd", &pars.at(0), &pars.at(1), &pars.at(2), &pars.at(3), trash, &col, &row) == 7)
        tmp_par_map[string(TString::Format("%02d%02d", col, row))] = pars;
      m_cal_parameters.push_back(tmp_par_map);
      fclose(fp);
    }
  }
  do_conversion = true;
  cout << "END OF READ PH CALIBRATION: CONVERT: " << do_conversion << endl;
  }

  bool CMSPixelHelper::GetStandardSubEvent(eudaq::EventSPC in, eudaq::StandardEventSP out) const {

    if (in->IsEORE() or out->IsEORE()){ // or in->GetEventN() % 10000 == 9999){
      cout << "Decoding statistics for detector " << m_detector << endl;
      pxar::Log::ReportingLevel() = pxar::Log::FromString("INFO");
      decoding_stats.dump();
    }
    // Check if we have BORE or EORE:
    if (in->IsBORE()) { return true; }

    // Check ROC type from event tags:
    if (m_roctype == 0x0) {
      EUDAQ_ERROR("Invalid ROC type\n");
      return false;
    }

    auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(in);
    // Check of we have more than one data block:
    if (ev->NumBlocks() > 1) {
      EUDAQ_ERROR("Only one data block is expected!");
      return false;
    }

    // Set decoder to reasonable verbosity (still informing about problems:
    pxar::Log::ReportingLevel() = pxar::Log::FromString("CRITICAL");

    // The pipeworks:
    evtSource src;
    passthroughSplitter splitter;
    dtbEventDecoder decoder;
    // todo: read this by a config file or even better, write it to the data!
    vector<float> offsets(16, decodingOffset);
    decoder.setBlackOffsets(offsets);
    dataSink < pxar::Event * > Eventpump;
    pxar::Event *evt;
    try {
      // Connect the data source and set up the pipe:
      src = evtSource(0, m_nplanes, 0, m_tbmtype, m_roctype);
      src >> splitter >> decoder >> Eventpump;

      // Transform from EUDAQ data, add it to the datasource:
      src.AddData(TransformRawData(ev->GetBlock(0)));
      // ...and pull it out at the other end:
      evt = Eventpump.Get();
      decoding_stats += decoder.getStatistics();
    }
    catch (std::exception &e) {
      EUDAQ_WARN("Decoding crashed at event " + to_string(in->GetEventNumber()) + ":");
      std::ofstream f;
      std::string runNumber = to_string(in->GetRunNumber());
      std::string filename = "Errors" + std::string(3 - runNumber.length(), '0') + runNumber + ".txt";
      f.open(filename, std::ios::out | std::ios::app);
      f << in->GetEventNumber() << "\n";
      f.close();
      std::cout << e.what() << std::endl;
      return false;
    }

    // Iterate over all planes and check for pixel hits:
    for (size_t roc = 0; roc < m_nplanes; roc++) {

        // We are using the event's "sensor" (m_detector) to distinguish DUT and REF:
      StandardPlane plane(roc, m_event_type, m_detector);

      // Initialize the plane size (zero suppressed), set the number of pixels
      // Check which carrier PCB has been used and book planes accordingly:
      if (m_rotated_pcb) { plane.SetSizeZS(ROC_NUMROWS, ROC_NUMCOLS, 0); }
      else { plane.SetSizeZS(ROC_NUMCOLS, ROC_NUMROWS, 0); }

      // Store all decoded pixels belonging to this plane:
      for (auto & it : evt->pixels) {
        if (it.roc() == roc) { /** Check if current pixel belongs on this plane: */
          double charge = do_conversion ? calc_vcal(it.roc(), it.column(), it.roc(), it.value()) : it.value();
          m_rotated_pcb ? plane.PushPixel(it.row(), it.column(), it.value(), charge) : plane.PushPixel(it.column(), it.row(), it.value(), charge);
        }
      }
      // Add plane and trigger phase to the output event:
      out->AddTriggerPhase(evt->triggerPhase());
      out->AddTriggerCount(evt->triggerCount());
      out->AddPlane(plane);
    }
    return true;
  }

  std::vector<uint16_t> CMSPixelHelper::TransformRawData(const std::vector<unsigned char> &block) {

    // Transform data of form char* to vector<int16_t>
    std::vector<uint16_t> rawData;

    int size = block.size();
    if(size < 2) { return rawData; }

    int i = 0;
    while(i < size-1) {
      uint16_t temp = ((uint16_t)block.data()[i+1] << 8) | block.data()[i];
      rawData.push_back(temp);
      i+=2;
    }
    return rawData;
  }

//  string CMSPixelHelper::get_event_type(const ConfigurationSPC & conf) {
////    string section_name = conf->GetCurrentSectionName();
//    string section_name = "REF";
//    if (section_name.find("REF") != string::npos) { return "REF";}
//    if (section_name.find("ANA") != string::npos) {return "ANA";}
//    else if (section_name.find("DIG") != string::npos) {return "DIG";}
//    else if (section_name.find("TRP") != string::npos) {return "TRP";}
//    return "DUT";
//  }

}
