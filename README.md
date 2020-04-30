This repository contains benchmarking code to test the efficiency of the [PGM-index](https://github.com/gvinciguerra/PGM-index) (see also the [project website](https://pgm.di.unipi.it)) with respect to the official implementation of RMI, developed by the Data Systems and AI Lab (DSAIL) at the MIT.
These experiments complement the ones published in:

> Paolo Ferragina and Giorgio Vinciguerra. [The PGM-index: a fully-dynamic compressed learned index with provable worst-case bounds](http://www.vldb.org/pvldb/vol13/p1162-ferragina.pdf). PVLDB, 13(8): 1162-1175, 2020.

The content of `results.txt` on our machine is the following:

```
CPU is:  Intel(R) Xeon(R) Gold 5118 CPU @ 2.30GHz
Dataset,RMI,PGM
data/books_200M_uint64,257,249
data/osm_cellids_200M_uint64,403,301
data/wiki_ts_200M_uint64,204,247
data/fb_200M_uint64,322,299
```

So on 3 out of 4 dataset the PGM-index is faster than RMI. We also note that the space improvement ranges from 1.8x (on fb) to 24.4x (on wiki). 

# Instructions to run the benchmark by [@RyanMarcus](https://github.com/RyanMarcus)

A repository to test PGM and RMI on different platforms using a much simpler benchmark harness than SOSD.

The only dependencies are:

* A C++ compiler with support for C++ 17
* `xz` compression tool (`pacman -S xz`)
* `zstd` compression tool (`pacman -S zstd`)
* `wget` to download files (`pacman -S wget`)

You can execute the benchmark with:
```bash
make results.txt
```

This should download the data, uncompress the RMIs, and record the benchmark data and your current CPU into the file results.txt.


If you want to reproduce the RMI parameters used, here are the commands to use on the RMI learner:
```
cargo run --release -- ../data/wiki_ts_200M_uint64 wiki cubic,linear 2097152 -d rmi_data/ -e
cargo run --release -- ../data/fb_200M_uint64 fb robust_linear,linear 2097152 -d rmi_data/ -e
cargo run --release -- ../data/books_200M_uint64 books linear_spline,linear 2097152 -d rmi_data/ -e
cargo run --release -- ../data/osm_cellids_200M_uint64 osm cubic,linear 2097152 -d rmi_data/ -e
```
