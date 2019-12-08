#ifndef EUDAQ_TTREEFILEWRITER_HH
#define EUDAQ_TTREEFILEWRITER_HH

#include <string>
#include <vector>
class TFile; class TTree;

namespace eudaq{

  class TTreeFileWriter : public FileWriter {
  public:
    TTreeFileWriter(std::string  out, const std::string & in);
    ~TTreeFileWriter() override;
    void WriteEvent(EventSPC ev) override;
    void SetBranchAddresses();
    void ClearVectors();
    void InitVectors();
    void FillVectors(const StandardEventSPC& stdev);
    static uint8_t FindNCMSPixels(const EventSPC& ev);

  private:
    std::string m_filepattern;
    uint32_t m_run_n;
    TFile * m_tfile;
    TTree * m_ttree;
    ConfigurationSPC m_config;
    uint8_t m_n_planes;
    uint8_t m_n_cms_pixels;
    bool m_init_vectors;

    /** Branches */
    uint32_t b_event_nr;
    double b_time_stamp_begin, b_time_stamp_end;
    std::vector<uint16_t> b_n_hits;
    std::vector<uint16_t> b_trigger_phase;
    std::vector<uint16_t> b_trigger_count;
    std::vector<std::vector<uint16_t>> * b_column;
    std::vector<std::vector<uint16_t>> * b_row;
    std::vector<std::vector<uint16_t>> * b_adc;
    std::vector<std::vector<float>> * b_charge;

  };
}

#endif //EUDAQ_TTREEFILEWRITER_HH
