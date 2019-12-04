#include "eudaq/StdEventConverter.hh"
#include "eudaq/StandardEvent.hh"
#include "eudaq/RawEvent.hh"
#include "eudaq/Logger.hh"
#include "eudaq/Configuration.hh"

#include "CMSPixelHelper.hh"

#include "iostream"
#include "bitset"

namespace eudaq {

  class CMSPixelANAConverterPlugin : public StdEventConverter {
  public:
    static const uint32_t m_id_factory = eudaq::cstr2hash("CMSPixelANA");

    bool Converting(EventSPC in, StandardEventSP out, ConfigurationSPC conf) const override;

  };

  namespace {
    auto dummy0 = eudaq::Factory<eudaq::StdEventConverter>::
    Register<CMSPixelANAConverterPlugin>(CMSPixelANAConverterPlugin::m_id_factory);
  }


  bool CMSPixelANAConverterPlugin::Converting(eudaq::EventSPC in, eudaq::StandardEventSP out, eudaq::ConfigurationSPC conf) const {
    static CMSPixelHelper * s_converter = nullptr;
    // The EventN is set by EUDAQ2 in background.
    // First Event (in a data-taking-run for each Producer) has EventN==0 and BORE flag by EUDAQ2 default (base class).
    // Assuming you always send config tag at begin of a run (0-EventN event).
    if (in->GetEventN() == 0) {
      if (s_converter) {
        std::cout << "WARN: global converter is replaced as a new 0-EventN Event arrival." << std::endl;
        std::cout << "WARN: it is fine if you just start a new data taking." << std::endl;
        delete s_converter;
        s_converter = nullptr;
        s_converter = new CMSPixelHelper(in, conf);
      } else {
        std::cout << "INFO: new global converter is created as a new 0-EventN Event arrival" << std::endl;
        s_converter = new CMSPixelHelper(in, conf);
      }
      return true; // in case you BORE has real detector data, remove this line.
    }
    if (!s_converter) {
      std::cerr << "ERROR: There must be a tagged 0-EventN Event which helps to create the global converter.\n";
      throw;
    }

    return s_converter->GetStandardSubEvent(in, out);

    //****************IMPORTANTE NOTE*************************************
    //a global converter means you are not able to run it for 2 devices data which has different configure.
  }
}
