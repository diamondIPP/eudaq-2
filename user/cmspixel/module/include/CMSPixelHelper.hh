#ifndef CMSPIXELHELPER_HH
#define CMSPIXELHELPER_HH

#include "Configuration.hh"
#include "StandardEvent.hh"
#include <vector>
#include "datatypes.h"

class TF1;

namespace eudaq {


  class CMSPixelHelper {

  private:
    uint8_t m_roctype{}, m_tbmtype{};
    size_t m_nplanes{};
    std::string m_detector;
    bool m_rotated_pcb{};
    std::string m_event_type;
    mutable pxar::statistics decoding_stats;
    bool do_conversion{};
    uint8_t decodingOffset{};
    static std::vector<uint16_t> TransformRawData(const std::vector<unsigned char> & block);

  public:
    float m_calibration_factor;
    CMSPixelHelper(const EventSPC& bore, const ConfigurationSPC& cnf);
    std::vector<std::map<std::string, std::vector<double>>> m_cal_parameters;
    TF1 * f_fit_function;

    void set_conversion(bool val) { do_conversion = val; }
    bool get_conversion() { return do_conversion; }
    double get_charge(double vcal) const { return vcal * m_calibration_factor; }
    float calc_vcal(uint16_t roc, uint16_t col, uint16_t row, uint16_t adc) const;
    std::string get_stats() { return decoding_stats.getString(); }

    void initialize(const EventSPC& bore);
    void read_ph_calibration(const EventSPC & bore);

    bool GetStandardSubEvent(eudaq::EventSPC in, eudaq::StandardEventSP out) const;

//    std::string get_event_type(const ConfigurationSPC & conf);
  };

}
#endif /*CMSPIXELHELPER_HH*/
