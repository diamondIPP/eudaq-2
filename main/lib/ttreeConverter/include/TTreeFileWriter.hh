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
    void SetBranches();
    void InitVectors();
    void InitTrees();
    void FillVectors(const StandardEventSPC& stdev);
    static uint8_t FindNCMSPixels(const EventSPC& ev);

  private:
    std::string m_filepattern;
    uint32_t m_run_n;
    TFile * m_tfile;
    TTree * m_event_tree;
    std::vector<TTree*> m_plane_trees;
    ConfigurationSPC m_config;
    uint8_t m_n_planes, m_n_telescope_planes, m_n_cms_pixels;
    bool m_init_vectors;
    uint16_t m_max_hits;

    /** Branches */
    uint32_t b_event_nr;
    uint64_t b_time_stamp_begin, b_time_stamp_end;
    std::vector<int> b_n_hits;
    std::vector<int*> b_column;
    std::vector<int*> b_row;
    std::vector<int*> b_adc;
    std::vector<int*> b_timing;
    std::vector<int*> b_hit_in_cluster;
    std::vector<double*> b_charge;
    std::vector<uint16_t> b_trigger_phase;
    std::vector<uint16_t> b_trigger_count;

  };
}

#endif //EUDAQ_TTREEFILEWRITER_HH
