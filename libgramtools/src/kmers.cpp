#include <algorithm>
#include <thread>

#include "fm_index.hpp"
#include "kmers.hpp"
#include "bidir_search_bwd.hpp"


#define MAX_THREADS 25


std::vector<uint8_t> encode_dna_bases(const std::string &dna_str) {
    std::vector<uint8_t> dna;
    for (const auto &base_str: dna_str) {
        int encoded_base;
        switch (base_str) {
            case 'A':
            case 'a':
                encoded_base = 1;
                break;
            case 'C':
            case 'c':
                encoded_base = 2;
                break;
            case 'G':
            case 'g':
                encoded_base = 3;
                break;
            case 'T':
            case 't':
                encoded_base = 4;
                break;
            default:
                break;
        }
        dna.emplace_back(encoded_base);
    }
    return dna;
}


std::string dump_kmer(const Kmer &kmer) {
    std::stringstream stream;
    for (const auto &base: kmer) {
        stream << (int) base;
        bool last_base = (&base == &kmer.back());
        if (!last_base)
            stream << ' ';
    }
    return stream.str();
}


std::string dump_sa_intervals(const SA_Intervals &sa_intervals) {
    std::stringstream stream;
    for (const auto &sa_interval: sa_intervals) {
        stream << sa_interval.first
               << " "
               << sa_interval.second;
        bool last_base = (&sa_interval == &sa_intervals.back());
        if (!last_base)
            stream << " ";
    }
    return stream.str();
}


std::string dump_kmer_in_ref_flag(const Kmer &kmer,
                                  const KmersRef &kmers_in_ref) {
    if (kmers_in_ref.count(kmer) != 0)
        return "1";
    return "0";
}


std::string dump_sites(const Kmer &kmer, const KmerSites &kmer_sites) {
    std::stringstream stream;
    for (const auto &all_sites: kmer_sites.at(kmer)) {
        for (const auto &sites: all_sites) {
            stream << sites.first << " ";
            for (const auto &allele: sites.second)
                stream << (int) allele << " ";
            stream << "@";
        }
        stream << "|";
    }
    return stream.str();
}


std::string dump_kmer_precalc_entry(const Kmer &kmer,
                                    const SA_Intervals &sa_intervals,
                                    const KmersRef &kmers_in_ref,
                                    const KmerSites &kmer_sites) {
    std::stringstream stream;
    stream << dump_kmer(kmer) << "|";
    stream << dump_kmer_in_ref_flag(kmer, kmers_in_ref) << "|";
    // additional bar because of reverse sa intervals
    stream << dump_sa_intervals(sa_intervals) << "||";
    stream << dump_sites(kmer, kmer_sites);
    return stream.str();
}


void dump_thread_result(std::ofstream &precalc_file,
                        const KmerIdx &kmers_sa_intervals,
                        const KmersRef &kmers_in_ref,
                        const KmerSites &kmer_sites) {

    for (const auto &kmer_sa_intervals: kmers_sa_intervals) {
        const auto &kmer = kmer_sa_intervals.first;
        const SA_Intervals &sa_intervals = kmers_sa_intervals.at(kmer);
        const std::string kmer_entry =
                dump_kmer_precalc_entry(kmer,
                                        sa_intervals,
                                        kmers_in_ref,
                                        kmer_sites);
        precalc_file << kmer_entry << std::endl;
    }
}


void calc_kmer_matches(KmerIdx &kmer_idx,
                       KmerSites &kmer_sites,
                       KmersRef &kmers_in_ref,
                       Kmers &kmers,
                       const uint64_t maxx,
                       const std::vector<int> &allele_mask,
                       const DNA_Rank &rank_all,
                       const FM_Index &fm_index) {

    for (auto &kmer: kmers) {
        kmer_idx[kmer] = SA_Intervals();
        kmer_sites[kmer] = Sites();

        bool delete_first_interval = false;
        bool kmer_precalc_done = false;

        if (kmer_idx[kmer].empty()) {
            kmer_idx[kmer].emplace_back(std::make_pair(0, fm_index.size()));

            Site empty_pair_vector;
            kmer_sites[kmer].push_back(empty_pair_vector);
        }

        bidir_search_bwd(kmer_idx[kmer], kmer_sites[kmer], delete_first_interval, kmer.begin(),
                         kmer.end(), allele_mask, maxx, kmer_precalc_done, rank_all, fm_index);

        if (kmer_idx[kmer].empty())
            kmer_idx.erase(kmer);

        if (!delete_first_interval)
            kmers_in_ref.insert(kmer);
    }
}


void *worker(void *st) {
    auto *th = (ThreadData *) st;
    calc_kmer_matches(*(th->kmer_idx),
                      *(th->kmer_sites),
                      *(th->kmers_in_ref),
                      *(th->kmers),
                      th->maxx,
                      *(th->allele_mask),
                      *(th->rank_all),
                      *(th->fm_index));
    return nullptr;
}


void generate_kmers_encoding_single_thread(const std::vector<int> &allele_mask,
                                           const std::string &kmer_fname,
                                           const uint64_t max_alphabet_num,
                                           const DNA_Rank &rank_all,
                                           const FM_Index &fm_index) {

    std::ifstream kmer_fhandle;
    kmer_fhandle.open(kmer_fname);

    Kmers kmers;
    std::string line;
    while (std::getline(kmer_fhandle, line)) {
        const Kmer &kmer = encode_dna_bases(line);
        kmers.emplace_back(kmer);
    }

    KmerIdx kmer_idx;
    KmerSites kmer_sites;
    KmersRef kmers_in_ref;

    calc_kmer_matches(kmer_idx,
                      kmer_sites,
                      kmers_in_ref,
                      kmers,
                      max_alphabet_num,
                      allele_mask,
                      rank_all,
                      fm_index);

    std::ofstream precalc_file;
    precalc_file.open(std::string(kmer_fname) + ".precalc");

    dump_thread_result(precalc_file,
                       kmer_idx,
                       kmers_in_ref,
                       kmer_sites);
}


void generate_kmers_encoding_threading(const std::vector<int> &allele_mask,
                                       const std::string &kmer_fname,
                                       const uint64_t max_alphabet_num,
                                       const int thread_count,
                                       const DNA_Rank &rank_all,
                                       const FM_Index &fm_index) {

    pthread_t threads[thread_count];
    struct ThreadData td[thread_count];
    Kmers kmers[thread_count];

    std::ifstream kmer_fhandle;
    kmer_fhandle.open(kmer_fname);

    int j = 0;
    std::string line;
    while (std::getline(kmer_fhandle, line)) {
        Kmer kmer = encode_dna_bases(line);
        kmers[j++].push_back(kmer);
        j %= thread_count;
    }

    KmerIdx kmer_idx[thread_count];
    KmerSites kmer_sites[thread_count];
    KmersRef kmers_in_ref[thread_count];

    for (int i = 0; i < thread_count; i++) {
        td[i].fm_index = &fm_index;
        td[i].kmer_idx = &kmer_idx[i];
        td[i].kmer_sites = &kmer_sites[i];
        td[i].allele_mask = &allele_mask;
        td[i].maxx = max_alphabet_num;
        td[i].kmers_in_ref = &kmers_in_ref[i];
        td[i].kmers = &kmers[i];
        td[i].thread_id = i;
        std::cout << "Starting thread: " << i << std::endl;
        pthread_create(&threads[i], nullptr, worker, &td[i]);
    }

    std::cout << "Joining threads" << std::endl;

    std::ofstream precalc_file;
    precalc_file.open(std::string(kmer_fname) + ".precalc");

    for (int i = 0; i < thread_count; i++) {
        void *status;
        pthread_join(threads[i], &status);
        dump_thread_result(precalc_file,
                           kmer_idx[i],
                           kmers_in_ref[i],
                           kmer_sites[i]);
    }
}


inline bool fexists(const std::string &name) {
    std::ifstream f(name.c_str());
    return f.good();
}


static inline std::string &ltrim(std::string &s) {
    //trim from start
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}


static inline std::string &rtrim(std::string &s) {
    // trim from end
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}


static inline std::string &trim(std::string &s) {
    // trim from both ends
    return ltrim(rtrim(s));
}


std::vector<std::string> split(const std::string &cad, const std::string &delim) {
    std::vector<std::string> v;
    uint64_t p, q, d;

    q = cad.size();
    p = 0;
    d = strlen(delim.c_str());
    while (p < q) {
        uint64_t posfound = cad.find(delim, p);
        std::string token;

        if (posfound >= 0)
            token = cad.substr(p, posfound - p);
        else
            token = cad.substr(p, cad.size() + 1);
        p += token.size() + d;
        trim(token);
        v.push_back(token);
    }
    return v;
}


bool parse_in_reference_flag(const std::string &in_reference_flag_str) {
    return in_reference_flag_str == "1";
}


Kmer parse_encoded_kmer(const std::string &encoded_kmer_str) {
    Kmer encoded_kmer;
    for (const auto &encoded_base: split(encoded_kmer_str, " ")) {
        const auto kmer_base = (uint8_t) std::stoi(encoded_base);
        encoded_kmer.push_back(kmer_base);
    }
    return encoded_kmer;
}


SA_Intervals parse_sa_intervals(const std::string &full_sa_intervals_str) {
    std::vector<std::string> split_sa_intervals = split(full_sa_intervals_str, " ");
    SA_Intervals sa_intervals;
    for (auto i = 0; i < split_sa_intervals.size(); i = i + 2) {
        auto sa_interval_start = (uint64_t) std::stoi(split_sa_intervals[i]);
        auto sa_interval_end = (uint64_t) std::stoi(split_sa_intervals[i + 1]);
        const SA_Interval &sa_interval = std::make_pair(sa_interval_start, sa_interval_end);
        sa_intervals.emplace_back(sa_interval);
    }
    return sa_intervals;
}


Site parse_site(const std::string &sites_part_str) {
    Site site;
    for (const auto &pair_i_v: split(sites_part_str, "@")) {
        std::vector<std::string> site_parts = split(pair_i_v, " ");
        if (site_parts.empty())
            continue;

        auto variant_site_marker = (VariantSiteMarker) std::stoi(site_parts[0]);

        std::vector<int> allele;
        for (uint64_t i = 1; i < site_parts.size(); i++) {
            const auto &allele_element = site_parts[i];
            if (!allele_element.empty())
                allele.push_back(std::stoi(allele_element));
        }

        site.emplace_back(VariantSite(variant_site_marker, allele));
    }
    return site;
}


void parse_kmer_index_entry(KmersData &kmers, const std::string &line) {
    const std::vector<std::string> &parts = split(line, "|");

    Kmer encoded_kmer = parse_encoded_kmer(parts[0]);
    const auto in_reference_flag = parse_in_reference_flag(parts[1]);
    if (!in_reference_flag)
        kmers.in_precalc.insert(encoded_kmer);

    const auto sa_intervals = parse_sa_intervals(parts[2]);
    if (!sa_intervals.empty())
        kmers.index[encoded_kmer] = sa_intervals;
    else
        return;

    Sites sites;
    // start at i=4 because of reverse sa intervals
    for (uint64_t i = 4; i < parts.size(); i++) {
        const auto site = parse_site(parts[i]);
        sites.push_back(site);
    }
    kmers.sites[encoded_kmer] = sites;
}


KmersData read_encoded_kmers(const std::string &encoded_kmers_fname) {
    std::ifstream fhandle;
    fhandle.open(encoded_kmers_fname);

    KmersData kmers;
    std::string line;
    while (std::getline(fhandle, line)) {
        parse_kmer_index_entry(kmers, line);
    }

    assert(!kmers.sites.empty());
    return kmers;
}


int get_thread_count() {
    int count_total = std::thread::hardware_concurrency();
    int selected_count = std::min(MAX_THREADS, count_total) - 1;
    if (selected_count < 1)
        return 1;
    return selected_count;
}


KmersData get_kmers(const std::string &kmer_fname,
                    const std::vector<int> &allele_mask,
                    const uint64_t maxx,
                    const DNA_Rank &rank_all,
                    const FM_Index &fm_index) {

    const auto encoded_kmers_fname = std::string(kmer_fname) + ".precalc";

    if (!fexists(encoded_kmers_fname)) {
        const int thread_count = get_thread_count();
        std::cout << "Precalculated kmers not found, generating them using "
                  << thread_count << " threads" << std::endl;
        generate_kmers_encoding_threading(allele_mask, kmer_fname, maxx, thread_count, rank_all, fm_index);
        std::cout << "Finished precalculating kmers" << std::endl;
    }

    std::cout << "Reading Kmers" << std::endl;
    const auto kmers = read_encoded_kmers(encoded_kmers_fname);
    return kmers;
}
