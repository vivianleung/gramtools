#include "genotype/infer/personalised_reference.hpp"
#include "genotype/infer/interfaces.hpp"
#include "prg/coverage_graph.hpp"

namespace gram::genotype {

    void Fasta::set_sample_info(std::string const& name, std::string const& desc){
        std::string to_assign_header{name};
        to_assign_header += std::string(" ") += desc;
        this->header = to_assign_header;
    };

    allele_vector get_all_alleles_to_paste(gt_site_ptr const &site, std::size_t ploidy) {
        allele_vector result(ploidy);
        auto all_site_alleles = site->get_alleles();
        GtypedIndices gts;
        if (site->is_null()) gts = GtypedIndices(ploidy, 0);
        else gts = std::get<GtypedIndices>(site->get_genotype());

        if (gts.size() != ploidy) throw InconsistentPloidyException();

        for (int j{0}; j < ploidy; j++) result.at(j) = all_site_alleles.at(gts.at(j));

        return result;
    }

    std::size_t get_ploidy(gt_sites const& gtyped_recs){
        // If all the sites are null genotyped, will return a ploidy of one.
        std::size_t ploidy{1};
        for (auto const &site : gtyped_recs) {
            if (!site->is_null()) {
                auto gt = std::get<GtypedIndices>(site->get_genotype());
                ploidy = gt.size();
                break;
            }
        }
        return ploidy;
    }

    unique_Fastas get_personalised_ref(gram::covG_ptr graph_root, gt_sites const &genotyped_records) {
        auto ploidy = get_ploidy(genotyped_records);
        Fastas p_refs(ploidy);
        gram::covG_ptr cur_Node{graph_root};

        while (cur_Node->get_edges().size() > 0){
           if (cur_Node->is_bubble_start()){
               auto site_index = siteID_to_index(cur_Node->get_site_ID());
               auto site = genotyped_records.at(site_index);
               auto to_paste_alleles = get_all_alleles_to_paste(site, ploidy);
               for (int i{0}; i < ploidy; i++)
                   p_refs.at(i).add_sequence(to_paste_alleles.at(i).sequence);

               cur_Node = site->get_site_end_node();
           }
           if (cur_Node->has_sequence()){
               auto sequence = cur_Node->get_sequence();
               for (int i{0}; i < ploidy; i++)
                   p_refs.at(i).add_sequence(sequence);
           }

           assert(cur_Node->get_edges().size() == 1);
           cur_Node = cur_Node->get_edges().at(0);
        }

        unique_Fastas unique_p_refs(p_refs.begin(), p_refs.end());

        return unique_p_refs;
    }

    bool operator <(const Fasta& first, const Fasta& second){
        return first.sequence < second.sequence;
    }

   std::ostream& operator<<(std::ostream& out_stream, const Fasta& input){
       out_stream << input.header;
        if (input.header.back() != '\n'){
           out_stream << std::endl;
        }

        auto seq_write = input.sequence.c_str();
        auto remaining = input.sequence.size();
        while (remaining > FASTA_LWIDTH){
           out_stream.write(seq_write, FASTA_LWIDTH);
           seq_write += FASTA_LWIDTH;
           remaining -= FASTA_LWIDTH;
           out_stream << std::endl;
        }

        out_stream.write(seq_write, remaining);
    }
}
