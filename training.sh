#!/bin/bash
samples=$1
nnFile=$2
layers=$3
N=$4
filters=$5
tmp=$6
notest=$7
[[ -z "$tmp" ]] && (hostname | grep '.edu'>/dev/null) && tmp="/run/user/$(id -u)" # andrew temp dirs
([[ -n "$tmp" ]] && [[ -e $tmp ]]) || tmp="/tmp"
exe=$(pwd)/$(dirname $0)/sgf-parse/build/sgfparse

[[ -n "$notest" ]] && notest="--notest"

renice -n 20 $$

maxProcs=`nproc`

echo "SAMPLES: $samples"
echo "MAX PROCS: $maxProcs"
echo "NN FILE: $nnFile"
echo "LAYERS: $layers"
echo "FILTERS: $filters"
echo "ITERATIONS: $N"

mkdir -p $(dirname $exe)
pushd $(dirname $exe)
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j $((`nproc` - 1))
popd

tgzDir="${tmp}/kgs-games"
gameDir="${tgzDir}/games"

mkdir -p $gameDir
pushd ${tgzDir}
[[ -e kgs.tar.gz ]] || wget https://dl.dropboxusercontent.com/u/7148996/kgs.tar.gz
cd $gameDir
tar xzf ../kgs.tar.gz
tar xzf test.tgz
popd

export gameDir
export tgzDir
export tmp
export exe

for n in `seq 1 $N`; do

    mkdir -p ${tmp}/feats
    pushd ${tmp}/feats

    rm *.txt
    rm -r */

    echo "Grabbing samples..."
    time (
    (for i in `seq 1 $samples`; do echo -n "$i;"; done) | \
        xargs -P $maxProcs -d ';' -n 1 bash -c '

            filenum=$0
            id=$(basename $(mktemp))
            mkdir -p $id
            cd $id
            kgsSet=`(for f in ${gameDir}/kgs-*.tgz; do echo $f; done) | shuf | head -n 1`
            d="feats-$(basename ${kgsSet%.tgz})"
            mkdir -p $d
            tar xzf $kgsSet
            sgfdir=`basename ${kgsSet%.tgz}`
            cd $sgfdir
            sgfFile="$((for f in $(pwd)/*.sgf; do echo $f; done) | shuf | \
                        head -n 1)"
            cd ..
            cd $d
            $exe --features $sgfFile >/dev/null 2>/dev/null
            featFile="$((for f in $(pwd)/*.txt; do echo $f; done) | shuf | \
                        head -n 1)"
            cd ../..
            cp $featFile .
            rm -r $id'

        wait

        echo "Done: $(ls | wc -l) samples"
        )


        popd

    if [[ -n "$nnFile" ]]; then
        layersarg="$layers"
        [[ -n "$layersarg" ]] && layersarg="--layers $layersarg"
        filtersarg="$filters"
        [[ -n "$filtersarg" ]] && filtersarg="--filters $filtersarg"
        $exe --train $nnFile $layersarg $filtersarg $notest $((for f in ${tmp}/feats/*; do echo $f; done) | shuf)
    fi

done

