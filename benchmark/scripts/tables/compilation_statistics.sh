#!bash
(
sed -n '1p' data/analysis/statistics/all-thompson-none.tex
for file in data/analysis/statistics/{all,sl}-{thompson,flat}-{none,cn,in}.tex; do
  sed -n '2p' ${file}
done
) | sed 's/ & /, /g' | sed 's/ \\\\//'
