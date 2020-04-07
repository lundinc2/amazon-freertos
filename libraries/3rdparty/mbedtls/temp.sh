git --no-pager diff $1 ~/Documents/mbedtls/$1

git --no-pager diff -u ~/Documents/amazon-freertos/libraries/3rdparty/mbedtls/$1 ~/Documents/mbedtls/$1 > patchfile.patch
cd ~/Documents/mbedtls/
git --no-pager diff -u mbedtls-2.16.0 mbedtls-2.16.5 -- $1 > ~/Documents/amazon-freertos/libraries/3rdparty/mbedtls/mbedtls-diff.patch
cd ~/Documents/amazon-freertos/libraries/3rdparty/mbedtls/
git --no-pager diff -u patchfile.patch mbedtls-diff.patch

read  -n 1 -p "Input Selection:" mainmenuinput
if [ "$mainmenuinput" = "y" ]; then
    patch ~/Documents/amazon-freertos/libraries/3rdparty/mbedtls/$1 patchfile.patch
    rm patchfile.patch
    rm mbedtls-diff.patch
    echo "Done patching"
fi
