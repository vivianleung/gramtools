[![Build Status](https://travis-ci.org/iqbal-lab-org/gramtools.svg?branch=master)](https://travis-ci.org/iqbal-lab-org/gramtools)
[![Docker Repository on Quay](https://quay.io/repository/iqballab/gramtools/status "Docker Repository on Quay")](https://quay.io/repository/iqballab/gramtools)

# gramtools
**TL;DR** genotype genetic variants using genome graphs.

Gramtools builds a genome graph (also known as population reference graph (PRG)) from a set of variants, either in the form of a reference sequence + VCF, or from a multiple-sequence alignment. 
Given sequence data from an individual, the graph is annotated with coverage and genotyped, producing a VCF (and a [jVCF](https://github.com/iqbal-lab-org/jVCF-spec)) of all the variation in the graph.
 
 A personalised reference genome for the sample is also inferred and new variation can be discovered 
 against it. You can then build a new graph from the initial and the new variants, and genotype this augmented graph.
 
 See [usage](#usage) for details, and checkout the [limitations](#limitations-and-recommendations) section for important
 limitations and recommendations.

## Contents

- [Install](#installrun)
  - [Container](#from-container)
  - [Source](#from-source)
- [Usage](#usage)
- [Limitations](#limitations-and-recommendations)
- [Docs](#documentation)
- [Contribute](#contributing)
- [Licence](#licence)

## Install/Run

### From container
The easiest way to run `gramtools` is via a container ([hosted on quay.io](https://quay.io/repository/iqballab/gramtools?tab=tags)).

To run with [Docker](https://www.docker.com/) or [Singularity](https://sylabs.io/):
```sh
tag="latest" # or, a specific released version
# Run with docker
docker run "quay.io/iqballab/gramtools:${tag}"
# Or run with singularity
URI="docker://quay.io/iqballab/gramtools:${tag}"
singularity exec "$URI" gramtools
```

### From source

#### Latest release

```sh
VERSION="1.7.0"
wget -O - "https://github.com/iqbal-lab-org/gramtools/releases/download/v${VERSION}/gramtools-${VERSION}.tar.gz" | tar xfz -
pip install "./gramtools-${VERSION}"
```
The latest release includes a precompiled binary for Linux. This will be used if it works on your machine, else it will get compiled during the installation.

We recommend installing inside a virtual environment:
```sh
python -m venv gram_ve && source gram_ve/bin/activate
pip install pip==20.0.2
pip install gramtools-${VERSION}
```

#### Latest source

```sh
pip install git+https://github.com/iqbal-lab-org/gramtools
```
This will always compile the binary.

#### Requirements

`Python >= 3.6` and `pip >= 20.0.2`

If the binary needs to be compiled, you also need `CMake >= 3.1.2` and a C++17 
compatible compiler: `g++ >=8` (tested) or `clang >=7` (untested).

For `gramtools discover` to work, you additionally need `R` and `Perl` available at runtime.

## Usage

```
Gramtools

Usage: 
    gramtools [-h] [--debug] [--force] subcommand
    
    Subcommands:
        gramtools build -o GRAM_DIR --ref REFERENCE
                       (--vcf VCF [VCF ...] | --prg PRG)
                       [--kmer_size KMER_SIZE]

        gramtools genotype -i GRAM_DIR -o GENO_DIR
                          --reads READS [READS ...] --sample_id SAMPLE_ID
                          [--ploidy {haploid,diploid}]
                          [--max_threads MAX_THREADS] [--seed SEED]

        gramtools discover -i GENO_DIR -o DISCO_DIR
                          [--reads READS [READS ...]]

        gramtools simulate --prg PRG
                           [--max_num_paths MAX_NUM_PATHS]
                           [--sample_id SAMPLE_ID] [--output_dir OUTPUT_DIR]
```

### Subcommands explained
1)  [build](https://github.com/iqbal-lab-org/gramtools/wiki/Commands%3A-build) - 
given a reference and a VCF or a prg file, builds an indexed graph.

    A prg file can be produced from a multiple-sequence alignment (MSA) by our tool
    [make_prg][make_prg].
    For genotyping complex regions (e.g. SNPs + SVs, or variants on multiple references),
    you must use a prg file made by make_prg.
    * `--kmer_size`: used for indexing the graph in preparation for
       `genotype`. higher `k` <=> faster `genotype`, but `build` output will consume more 
       disk space.

2)  [genotype](https://github.com/iqbal-lab-org/gramtools/wiki/Commands%3A-genotype) - 
    map reads to a graph generated in `build` and genotype the graph. Produces genotype calls (VCF)
    and a personalised reference genome (fasta).
    * `--reads`: 1+ reads files in (fasta/fastq/sam/bam/cram) format
    * `--sample_id`: displayed in VCF & personalised reference outputs

3) [discover](https://github.com/iqbal-lab-org/gramtools/wiki/Commands%3A-discover) - 
discovers new variation against the personalised reference genome from `genotype` using
 one or more variant callers (currently: cortex).
 
4) simulate- samples paths through a prg, producing a fasta of the paths and a genotyped JSON
of the variant bubbles the path went through.
    * `--prg`: a prg file as output by `build`

## Limitations and recommendations

* gramtools is primarily a genotyper, not a variant caller. Variant discovery 
  with the `discover` command relies on existing tools
* gramtools is designed to use short, low-error rate reads only (e.g.
Illumina). We recommend trimming adapters off reads (e.g. using [trimmomatic](http://www.usadellab.org/cms/?page=trimmomatic)) before genotyping with gramtools.
* gramtools currently performs exact matching of reads only, so relatively high read coverage (e.g.
  \>20X) is recommended
* gramtools does not currently genotype copy-number variants (CNVs) or samples with mixed ploidy
* Building a graph from MSAs with our tool [make_prg][make_prg] is not easy to scale
  whole-genome. We are working on this
  ([#130](https://github.com/iqbal-lab-org/gramtools/issues/130))

## Documentation

Examples, documentation, and planned future enhancements can be found in the [wiki](https://github.com/iqbal-lab-org/gramtools/wiki).

For the C++ source code, [doxygen](http://doxygen.nl/) formatted documentation can be generated by running 
```doxygen doc/Doxyfile.in```
from inside the gramtools directory.

The documentation gets generated in doc/html/index.html and provides a useful reference for all files, classes, functions and data structures in gramtools.

## Contributing

Please refer to the [developers wiki page](https://github.com/iqbal-lab-org/gramtools/wiki/Developers%3A-tips).

## License

MIT


[make_prg]: https://github.com/iqbal-lab-org/make_prg
