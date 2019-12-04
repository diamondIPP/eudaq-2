#include <utility>

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

  CMSPixelHelper::CMSPixelHelper(string event_type) : do_conversion(true), m_event_type(move(event_type)) {
    m_conv_cfg = new Configuration("");
    roc_calibrations = {{"psi46v2",           65},
                        {"psi46digv21respin", 47},
                        {"proc600",           47}};
    f_fit_function = new TF1("fitfunc", "[3]*(TMath::Erf((x-[0])/[1])+[2])", -4096, 4096);
  }

  float CMSPixelHelper::get_charge(eudaq::VCALDict d, float val, float factor) const {

    for (uint8_t i(0); i < 4; i++)
      f_fit_function->SetParameter(i, d.pars.at(i));
    return d.calibration_factor * f_fit_function->GetX(val);
  }

  void CMSPixelHelper::initialize(const EventSPC & bore, const ConfigurationSPC & cnf) {
    DeviceDictionary *devDict;
    std::string roctype = bore->GetTag("ROCTYPE", "");
    std::string tbmtype = bore->GetTag("TBMTYPE", "tbmemulator");

    m_detector = bore->GetTag("DETECTOR", "");
    std::string pcbtype = bore->GetTag("PCBTYPE", "");
    m_rotated_pcb = pcbtype.find("-rot") != std::string::npos;

    // Get the number of planes:
    m_nplanes = bore->GetTag("PLANES", 1);

    m_roctype = devDict->getInstance()->getDevCode(roctype);
    m_tbmtype = devDict->getInstance()->getDevCode(tbmtype);

    if (m_roctype == 0x0)
      EUDAQ_ERROR("Roctype" + to_string((int) m_roctype) + " not propagated correctly to CMSPixelConverterPlugin");
    read_ph_calibration(cnf);

    m_conv_cfg->SetSection("Converter.telescopetree");
    decodingOffset = m_conv_cfg->Get("decoding_offset", 25);

    std::cout << "CMSPixel Converter initialized with detector " << m_detector << ", Event Type " << m_event_type
              << ", TBM type " << tbmtype << " (" << static_cast<int>(m_tbmtype) << ")"
              << ", ROC type " << roctype << " (" << static_cast<int>(m_roctype) << ")" << std::endl;
  }

  void CMSPixelHelper::read_ph_calibration(const ConfigurationSPC & cnf) {
    std::cout << "TRY TO READ PH CALIBRATION DATA... ";
    bool foundData(false);
    std::string roctype = cnf->Get("roctype", "roctype", "");
    bool is_digital = roctype.find("dig") != string::npos;
    string fname = m_conv_cfg->Get("phCalibrationFile", "");
    if (fname == "") fname = cnf->Get("phCalibrationFile", "");
    if (fname == "") {
      fname = cnf->Get("dacFile", "");
      size_t found = fname.find_last_of("/");
      fname = fname.substr(0, found);
    }
    fname += (is_digital) ? "/phCalibrationFitErr" : "/phCalibrationGErfFit";
    std::string i2c = cnf->Get("i2c", "i2caddresses", "0");
    string section_name = cnf->GetCurrentSectionName();
    if (section_name.find("REF") != string::npos) foundData = read_ph_calibration_file("REF", fname, i2c, roc_calibrations.at(roctype));
    else if (section_name.find("ANA") != string::npos) foundData = read_ph_calibration_file("ANA", fname, i2c, roc_calibrations.at(roctype));
    else if (section_name.find("DIG") != string::npos) foundData = read_ph_calibration_file("DIG", fname, i2c, roc_calibrations.at(roctype));
    else if (section_name.find("TRP") != string::npos) foundData = read_ph_calibration_file("TRP", fname, i2c, roc_calibrations.at(roctype));
    else foundData = read_ph_calibration_file("DUT", fname, i2c, roc_calibrations.at(roctype));
    /** only do a conversion if we found data */
    do_conversion = foundData;
  }

  bool CMSPixelHelper::read_ph_calibration_file(std::string roc_type, std::string fname, std::string i2cs, float factor) {
    std::vector<std::string> vec_i2c = split(i2cs, " ");
    size_t nRocs = vec_i2c.size();

    // getting vcal-charge translation from file
    vector<float> pars;
    int row, col;
    std::string dump;
    char trash[30];
    for (uint16_t iroc = 0; iroc < nRocs; iroc++) {
      std::string i2c = vec_i2c.at(iroc);
      FILE *fp;
      char *line = nullptr;
      size_t len = 0;
      ssize_t read;

      TString filename = fname;
      filename += "_C" + i2c + ".dat";
      std::cout << "Filename in CMSPixelHelper: " << filename << std::endl;
      fp = fopen(filename, "r");

      VCALDict tmp_vcaldict;
      if (!fp) {
//          std::cout <<  "  DID NOT FIND A FILE TO GO FROM ADC TO CHARGE!!" << std::endl;
        return false;
      } else {
        // jump to fourth line
        for (uint8_t i = 0; i < 3; i++) if (!getline(&line, &len, fp)) return false;

        int q = 0;
        while (fscanf(fp, "%f %f %f %f %s %d %d", &pars.at(0), &pars.at(1), &pars.at(2), &pars.at(3), trash, &col, &row) == 7) {
          tmp_vcaldict.pars = pars;
          tmp_vcaldict.calibration_factor = factor;
          std::string identifier = roc_type + std::string(TString::Format("%01d%02d%02d", iroc, row, col));
          q++;
          vcal_vals[identifier] = tmp_vcaldict;
        }
        fclose(fp);
      }
    }
    return true;
  }

  bool CMSPixelHelper::GetStandardSubEvent(eudaq::EventSPC in, eudaq::StandardEventSP out) const {

    /** If we receive the EORE print the collected statistics: */
    if (out->IsEORE()) {
      std::cout << "Decoding statistics for detector " << m_detector << std::endl;
      pxar::Log::ReportingLevel() = pxar::Log::FromString("INFO");
      decoding_stats.dump();
    }

    // Check if we have BORE or EORE:
    if (in->IsBORE() || in->IsEORE()) { return true; }

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
    decoder.setOffset(decodingOffset);
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
      plane.SetTrigCount(evt->triggerCount());
      plane.SetTrigPhase(evt->triggerPhase());

      // Initialize the plane size (zero suppressed), set the number of pixels
      // Check which carrier PCB has been used and book planes accordingly:
      if (m_rotated_pcb) { plane.SetSizeZS(ROC_NUMROWS, ROC_NUMCOLS, 0); }
      else { plane.SetSizeZS(ROC_NUMCOLS, ROC_NUMROWS, 0); }

      // Store all decoded pixels belonging to this plane:
      for (std::vector<pxar::pixel>::iterator it = evt->pixels.begin(); it != evt->pixels.end(); ++it) {
        // Check if current pixel belongs on this plane:
        if (it->roc() == roc) {
          std::string identifier = (std::string) m_detector + (std::string) TString::Format("%01zu%02d%02d", roc, it->row(), it->column());

          float charge;
          if (do_conversion) {
            charge = get_charge(vcal_vals.find(identifier)->second, it->value());
            if (charge < 0) {
              EUDAQ_WARN(std::string("Invalid cluster charge -" + to_string(charge) + "/" + to_string(it->value())));
              charge = 0;
            }
          } else
            charge = it->value();

          //std::cout << "filling charge " <<it->value()<<" "<< charge << " "<<factor<<" "<<identifier<<std::endl;
          if (m_rotated_pcb) { plane.PushPixel(it->row(), it->column(), charge /*it->value()*/); }
          else { plane.PushPixel(it->column(), it->row(), charge /*it->value()*/); }
        }
      }

      // Add plane to the output event:
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

  CMSPixelHelper::CMSPixelHelper(const EventSPC &bore, const ConfigurationSPC &cnf) {

    initialize(bore, cnf);
  }

}
