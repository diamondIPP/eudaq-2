#include "eudaq/StdEventConverter.hh"
#include "eudaq/StandardEvent.hh"
#include "eudaq/RawEvent.hh"
#include "eudaq/Logger.hh"
#include "eudaq/Configuration.hh"

#include "CMSPixelHelper.hh"

#include "iostream"
#include "bitset"

namespace eudaq {

  class CMSPixelREFConverterPlugin : public StdEventConverter {
  public:
    static const uint32_t m_id_factory = eudaq::cstr2hash("CMSPixelREF");

    bool Converting(EventSPC in, StandardEventSP out, ConfigurationSPC conf) const override;

    virtual void SetConfig(Configuration * conv_cfg) { m_converter->set_config(conv_cfg); }
    virtual std::string GetStats() { return m_converter->get_stats(); }
    virtual void set_conversion(bool val){m_converter->set_conversion(val);}
    virtual bool get_conversion(){return m_converter->get_conversion();}
    virtual void Initialize(EventSPC bore, ConfigurationSPC cnf) const { m_converter->initialize(bore, cnf); }

  private:
    CMSPixelHelper * m_converter;
  };

  namespace{
    auto dummy0 = eudaq::Factory<eudaq::StdEventConverter>::
    Register<CMSPixelREFConverterPlugin>(CMSPixelREFConverterPlugin::m_id_factory);
  }

  bool CMSPixelREFConverterPlugin::Converting(eudaq::EventSPC in, eudaq::StandardEventSP out, eudaq::ConfigurationSPC conf) const {

    if (in->IsBORE() or in->GetTriggerN() == 0)
      *m_converter = CMSPixelHelper(in, conf);
    return m_converter->GetStandardSubEvent(in, out);
  }

}