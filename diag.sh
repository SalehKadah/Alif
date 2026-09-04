set -u
ROOT="$(pwd)"
PKG="$(mktemp -d)"; cp winBuild/x64/Release/alif.exe "$PKG/"; cp -r library examples "$PKG/"
cd "$PKG"
printf 'اطبع("سليم")\n' > ت.الف

r(){ echo "== $1"; shift; "$@" > o.txt 2>&1; echo "   rc=$?"; echo "   out:[$(cat o.txt | head -3 | tr -d '\r')]"; }

r "A: مثال بلا مدخل"        ./alif.exe examples/PrimeNumbers.الف
r "B: برنامج صغير"          ./alif.exe ت.الف
echo "== C: برنامج صغير بلا إعادة توجيه"; ./alif.exe ت.الف; echo "   rc=$?"
echo "== D: عبر cmd"; cmd //c "alif.exe ت.الف"; echo "   rc=$?"
echo "== E: مدخل من ملف"; printf '30\n' > in.txt; ./alif.exe examples/PrimeNumbers.الف < in.txt > o.txt 2>&1; echo "   rc=$?"; head -3 o.txt | tr -d '\r'
echo "== F: مسار مطلق"; ./alif.exe "$PKG/ت.الف" > o.txt 2>&1; echo "   rc=$?"; head -3 o.txt
