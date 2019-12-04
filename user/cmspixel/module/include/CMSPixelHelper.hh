#ifndef CMSPIXELHELPER_HH
#define CMSPIXELHELPER_HH

#include "Configuration.hh"
#include "StandardEvent.hh"
#include <vector>
#include "datatypes.h"

class TF1;

namespace eudaq {

  struct VCALDict {
    int row;
    int col;
    std::vector<float> pars;
    float calibration_factor;
  };


  class CMSPixelHelper {

  private:
    uint8_t m_roctype{}, m_tbmtype{};
    size_t m_planeid{};
    size_t m_nplanes{};
    std::string m_detector;
    bool m_rotated_pcb{};
    std::string m_event_type;
    mutable pxar::statistics decoding_stats;
    bool do_conversion{};
    Configuration * m_conv_cfg{};
    uint8_t decodingOffset{};
    static std::vector<uint16_t> TransformRawData(const std::vector<unsigned char> & block);

  public:
    std::map<std::string, float > roc_calibrations;
    CMSPixelHelper(const EventSPC& bore, const ConfigurationSPC& cnf);
    std::map< std::string, VCALDict> vcal_vals;
    TF1 * f_fit_function{};

    void set_conversion(bool val) { do_conversion = val; }
    void set_config(Configuration * conv_cfg) { m_conv_cfg = conv_cfg; }
    bool get_conversion() { return do_conversion; }
    float get_charge(VCALDict d, float val, float factor = 65.) const;
    std::string get_stats() { return decoding_stats.getString(); }

    void initialize(const EventSPC& bore, const ConfigurationSPC& cnf);
    void read_ph_calibration(const ConfigurationSPC & cnf);
    bool read_ph_calibration_file(std::string roc_type, std::string fname, std::string i2cs, float factor);

    bool GetStandardSubEvent(eudaq::EventSPC in, eudaq::StandardEventSP out) const;

    std::string get_event_type(const ConfigurationSPC & conf);
  };

}
#endif /*CMSPIXELHELPER_HH*/
