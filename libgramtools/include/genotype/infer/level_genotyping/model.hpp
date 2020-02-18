#ifndef LVLGT_MODEL
#define LVLGT_MODEL

#include "genotype/parameters.hpp"
#include "genotype/quasimap/coverage/types.hpp"
#include "site.hpp"
#include "probabilities.hpp"

using namespace gram;
using poisson_pmf = gram::genotype::infer::probabilities::PoissonLogPmf;
using poisson_pmf_ptr = poisson_pmf*;

namespace gram::genotype::infer {
    using numCredibleCounts = std::size_t;
    using multiplicities = std::vector<bool>;
    using likelihood_map = std::multimap<double, GtypedIndices, std::greater<double>>;
    using memoised_coverages = std::map<AlleleIds, allele_coverages>;

    struct likelihood_related_stats {
        double mean_cov_depth,
                mean_pb_error, log_mean_pb_error,
                log_no_zero, log_no_zero_half_depth;
        CovCount credible_cov_t; /**< minimum coverage count to qualify as actual coverage (per-base)*/
        mutable poisson_pmf poisson_full_depth;
        mutable poisson_pmf poisson_half_depth;
    };

    /**
    Genotyping model using:
      * coverage equivalence-classes
      * alternative alleles all at same nesting level
      * genotype confidence using likelihood ratios
      * invalidation of nested bubbles
    */
    class LevelGenotyperModel : GenotypingModel {
        allele_vector *alleles;
        GroupedAlleleCounts const *gp_counts;
        Ploidy ploidy;
        likelihood_related_stats const *l_stats;

        // Computed at construction time
        PerAlleleCoverage haploid_allele_coverages; /**< Coverage counts compatible with single alleles */
        PerAlleleCoverage singleton_allele_coverages; /**< Coverage counts unique to single alleles */
        memoised_coverages computed_coverages;
        std::size_t total_coverage;

        // Computed at run time
        likelihood_map likelihoods; // Store highest likelihoods first
        std::shared_ptr <LevelGenotypedSite> genotyped_site; // What the class will build

    public:
        LevelGenotyperModel() : gp_counts(nullptr) {}

        /**
         * @param ignore_ref_allele if true, the ref allele was not produced naturally,
         * and we do not consider it for genotyping
         */
        LevelGenotyperModel(allele_vector const *input_alleles, GroupedAlleleCounts const *gp_counts,
                            Ploidy ploidy, likelihood_related_stats const *l_stats,
                            bool ignore_ref_allele = false);


        /***********************
         **** Preparations *****
         ***********************/
        std::size_t count_total_coverage(GroupedAlleleCounts const &gp_counts);

        std::vector<bool> get_haplogroup_multiplicities(allele_vector const &input_alleles);

        void set_haploid_coverages(GroupedAlleleCounts const &input_gp_counts, AlleleId num_haplogroups);
        /**
         * Alleles with no sequence correspond to direct deletions.
         * In this case they get assigned coverage by this function,
         * using the grouped allele coverages, as if they had a single base.
         */
        void assign_coverage_to_empty_alleles(allele_vector &input_alleles);


        /***********************
         ****  Likelihoods *****
         ***********************/

        /**
         * Counts the number of positions in an allele with coverage above threshold `credible_cov_t`.
         * This threshold is the coverage at which true coverage is more likely than erroneous (sequencing error-based)
         * coverage.
         */
        numCredibleCounts count_credible_positions(CovCount const &credible_cov_t, Allele const &allele);

        /**
         * Haploid genotype likelihood
         */
        void compute_haploid_log_likelihoods(allele_vector const &input_alleles);

        /**
         * Diploid homozygous
         */
        void compute_homozygous_log_likelihoods(allele_vector const &input_alleles,
                                                multiplicities const &haplogroup_multiplicities);

        /**
         * Diploid. Because of the large possible number of diploid combinations,
         * (eg for 10 alleles, 45), we only consider for combination those alleles
         * that have at least one unit of coverage unique to them.
         */
        void compute_heterozygous_log_likelihoods(allele_vector const &input_alleles,
                                                  multiplicities const &haplogroup_multiplicities);
        /**
         * For producing the diploid combinations.
         * Credit: https://stackoverflow.com/a/9430993/12519542
         */
        std::vector <GtypedIndices> get_permutations(const GtypedIndices &indices, std::size_t const subset_size);

        /***********************
         ****   Coverages   ****
         ***********************/
        /**
         *
         * Note: Due to nesting, the alleles can be from the same haplogroup; in which case, they have the same
         * haploid coverage, and they get assigned half of it each.
         */
        std::pair<double, double> compute_diploid_coverage(GroupedAlleleCounts const &gp_counts, AlleleIds ids,
                                                           multiplicities const &haplogroup_multiplicities);

        std::pair<double, double> diploid_cov_same_haplogroup(AlleleIds const &ids, multiplicities const &hap_mults);
        std::pair<double, double>
        diploid_cov_different_haplogroup(GroupedAlleleCounts const &gp_counts, AlleleIds const &ids,
                                         multiplicities const &hap_mults);


        /***********************
         ****  Make result *****
         ***********************/

        void CallGenotype(allele_vector const *input_alleles, bool ignore_ref_allele, multiplicities hap_mults);

        AlleleIds get_haplogroups(allele_vector const &alleles, GtypedIndices const &gtype) const;

        /**
         * Express genotypes as relative to chosen alleles.
         * For eg, {0, 2, 4} in the original set of possible alleles goes to {0, 1, 2} in the 3 called alleles (yes,
         * this is Triploid example).
         */
        GtypedIndices rescale_genotypes(GtypedIndices const &genotypes);


        // Getters
        PerAlleleCoverage const &get_haploid_covs() const { return haploid_allele_coverages; }
        PerAlleleCoverage const &get_singleton_covs() const { return singleton_allele_coverages; }
        likelihood_map const &get_likelihoods() const { return likelihoods; }

        gt_site_ptr get_site() override { return std::static_pointer_cast<gt_site>(genotyped_site); }
        gtype_information get_site_gtype_info() {return genotyped_site->get_all_gtype_info();}
    };
}

#endif //LVLGT_MODEL