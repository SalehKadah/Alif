set -u
ROOT="$(pwd)"
PKG="$(mktemp -d)"; cp winBuild/x64/Release/alif.exe "$PKG/"; cp -r library examples "$PKG/"
cd "$PKG"
printf 'اطبع("سليم")\n' > t.alif

r(){ echo "== $1"; shift; "$@" > o.txt 2>&1; echo "   rc=$?"; echo "   out:[$(cat o.txt | head -3 | tr -d '\r')]"; }

r "A: مثال بلا مدخل"        ./alif.exe examples/PrimeNumbers.alif
r "B: برنامج صغير"          ./alif.exe t.alif
echo "== C: برنامج صغير بلا إعادة توجيه"; ./alif.exe t.alif; echo "   rc=$?"
echo "== D: عبر cmd"; cmd //c "alif.exe t.alif"; echo "   rc=$?"
echo "== E: مدخل من ملف"; printf '30\n' > in.txt; ./alif.exe examples/PrimeNumbers.alif < in.txt > o.txt 2>&1; echo "   rc=$?"; head -3 o.txt | tr -d '\r'
echo "== F: مسار مطلق"; ./alif.exe "$PKG/t.alif" > o.txt 2>&1; echo "   rc=$?"; head -3 o.txt
