#!/bin/bash -x
# Script creates a "vectrexy" folder root directory, populates it, and zips it as vectrexy.zip

package_name=$1
package_base_url=$2

root_dir=$(pwd)
output_dir=$root_dir/$package_name
data_zip_url=$package_base_url/data.zip

mkdir -p $output_dir
cp $root_dir/build/vectrexy $output_dir
cp $root_dir/bios_rom.bin $output_dir
cp $root_dir/README.md $output_dir
cp $root_dir/COMPAT.md $output_dir
cp $root_dir/LICENSE.txt $output_dir

git describe > $output_dir/version.txt

curl -sS $data_zip_url > data.zip
unzip data.zip -d $output_dir
rm data.zip

pushd $output_dir
rm $root_dir/${package_name}.zip
zip -r $root_dir/${package_name}.zip *
popd
