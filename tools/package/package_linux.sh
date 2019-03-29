#!/bin/bash -x
# Script creates a "vectrexy" folder root directory, populates it, and zips it as vectrexy.zip

set -e

package_name=$1
package_base_url=$2

root_dir=$(pwd)
output_dir=$root_dir/$package_name
data_zip_url=$package_base_url/data.zip

mkdir -p $output_dir
mkdir -p $output_dir/docs
mkdir -p $output_dir/data/bios

cp $root_dir/build/vectrexy $output_dir
cp -r $root_dir/docs $output_dir
cp -r $root_dir/data/bios $output_dir/data
cp $root_dir/README.md $output_dir
cp $root_dir/LICENSE.txt $output_dir

git describe > $output_dir/version.txt

curl -L $data_zip_url -o data.zip
unzip data.zip -d $output_dir
rm data.zip

pushd $output_dir
rm -f $root_dir/${package_name}.zip
zip -r $root_dir/${package_name}.zip *
popd
