#include "gtest/gtest.h"
#include "genotype/infer/genotyping_models.hpp"

using namespace gram::genotype::infer;

TEST(HaploidCoverages, GivenSingletonCountsOnly_CorrectHaploidAndSingletonCovs){
   GroupedAlleleCounts  gp_covs{
                   {{0}, 5},
                   {{1}, 10},
                   {{3}, 1}
   };

   LevelGenotyper gtyper;
   gtyper.set_haploid_coverages(gp_covs, 4);
   PerAlleleCoverage expected_haploid_cov{5, 10, 0, 1};
   EXPECT_EQ(gtyper.get_haploid_covs(), expected_haploid_cov);
   EXPECT_EQ(gtyper.get_singleton_covs(), expected_haploid_cov);
}


TEST(HaploidCoverages, GivenMultiAllelicClasses_CorrectHaploidAndSingletonCovs){
    GroupedAlleleCounts  gp_covs{
            {{0}, 5},
            {{0, 1}, 4},
            {{1}, 10},
            {{2, 3}, 1}
    };

    LevelGenotyper gtyper;
    gtyper.set_haploid_coverages(gp_covs, 4);

    PerAlleleCoverage expected_haploid_cov{9, 14, 1, 1};
    PerAlleleCoverage expected_singleton_cov{5, 10, 0, 0};

    EXPECT_EQ(gtyper.get_haploid_covs(), expected_haploid_cov);
    EXPECT_EQ(gtyper.get_singleton_covs(), expected_singleton_cov);
}

TEST(DiploidCoverages, GivenMultiAllelicClasses_CorrectDiploidCovs){
    AlleleIds ids{0, 1}; // We want coverages of alleles 0 and 1

    GroupedAlleleCounts  gp_covs{
            {{0}, 7},
            {{0, 1}, 4},
            {{1}, 20},
            {{0, 3}, 3},
            {{2, 3}, 1}
    };

    // We have 10 units uniquely on 0, 20 uniquely on 1, and 4 shared between them.
    // These 4 should get dispatched in ratio 1:2 to alleles 0:1 (cf iqbal-lab-org/minos)

    LevelGenotyper gtyper;
    gtyper.set_haploid_coverages(gp_covs, 4);
    auto diploid_covs = gtyper.compute_diploid_coverage(gp_covs, ids);
    EXPECT_FLOAT_EQ(diploid_covs.first, 10 + 4/3.);
    EXPECT_FLOAT_EQ(diploid_covs.second, 20 + 8/3.);
}

TEST(DiploidCoverages, GivenOnlyMultiAllelicClasses_CorrectDiploidCovs){
    AlleleIds ids{0, 1}; // We want coverages of alleles 0 and 1

    GroupedAlleleCounts  gp_covs{
            {{0, 1}, 3},
            {{2, 3}, 1}
    };

    // Edge case where singleton allele coverages are all 0
    // Then shared coverage should get dispatched equally (1:1 ratio)

    LevelGenotyper gtyper;
    gtyper.set_haploid_coverages(gp_covs, 4);
    auto diploid_covs = gtyper.compute_diploid_coverage(gp_covs, ids);
    EXPECT_FLOAT_EQ(diploid_covs.first, 1.5);
    EXPECT_FLOAT_EQ(diploid_covs.second, 1.5);
}

TEST(CountCrediblePositions, GivenAlleleWithCredibleAndNonCrediblePositions_ReturnCrediblePositions){
   Allele test_allele{
      "ATCGCCG",
      {0, 0, 2, 3, 3, 5, 4},
      0
   };

   LevelGenotyper gtyper;
   auto num_credible = gtyper.count_credible_positions(3, test_allele);
   EXPECT_EQ(num_credible, 4);
}