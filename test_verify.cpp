// Independent verifier for the ting-pai generation core logic.
// Reuses the REAL functions from the training program (main is renamed).
#define main original_main_is_disabled
#include "qingyise_tingpai_train.cpp"
#undef main

#include <array>
#include <map>

// ---------------------------------------------------------------------------
// Truly independent reference: brute force over ALL possible winning shapes.
// A standard win = 1 pair + 4 melds (triplet or run). Enumerate every multiset
// of 4 melds (9 triplet + 7 run types) and every pair.
// ---------------------------------------------------------------------------
static vector<array<int, 10>> g_meldCombos;
static map<array<int, 10>, bool> g_meldSet;

static void buildMeldCombos()
{
    vector<array<int, 10>> types;
    for (int i = 1; i <= 9; ++i)
    {
        array<int, 10> c{};
        c[i] = 3;
        types.push_back(c);
    }
    for (int i = 1; i <= 7; ++i)
    {
        array<int, 10> c{};
        c[i] = c[i + 1] = c[i + 2] = 1;
        types.push_back(c);
    }

    size_t n = types.size();
    for (size_t a = 0; a < n; ++a)
        for (size_t b = a; b < n; ++b)
            for (size_t c = b; c < n; ++c)
                for (size_t d = c; d < n; ++d)
                {
                    array<int, 10> sum{};
                    bool ok = true;
                    for (int k = 0; k < 10; ++k)
                    {
                        sum[k] = types[a][k] + types[b][k] + types[c][k] + types[d][k];
                        if (sum[k] > 4)
                            ok = false;
                    }
                    if (ok)
                    {
                        g_meldCombos.push_back(sum);
                        g_meldSet[sum] = true;
                    }
                }
}

static array<int, 10> toCounts(const vector<int> &tiles)
{
    array<int, 10> c{};
    for (int t : tiles)
        c[t]++;
    return c;
}

static bool refWinStandard(const vector<int> &tiles)
{
    if (tiles.size() != 14)
        return false;
    array<int, 10> need = toCounts(tiles);
    for (int p = 1; p <= 9; ++p)
    {
        if (need[p] < 2)
            continue;
        array<int, 10> rest = need;
        rest[p] -= 2;
        if (g_meldSet.count(rest))
            return true;
    }
    return false;
}

// Seven pairs: seven DISTINCT pairs. Four identical tiles (dragon seven
// pairs) must NOT count, per the training rules.
static bool refWinSevenPairs(const vector<int> &tiles)
{
    if (tiles.size() != 14)
        return false;
    array<int, 10> c = toCounts(tiles);
    int pairs = 0;
    for (int k = 1; k <= 9; ++k)
    {
        if (c[k] == 0)
            continue;
        if (c[k] != 2)
            return false;
        ++pairs;
    }
    return pairs == 7;
}

static bool refWin(const vector<int> &tiles)
{
    return refWinStandard(tiles) || refWinSevenPairs(tiles);
}

static bool inBand(int waits, Difficulty d)
{
    const WaitSpan s = waitSpanFor(d);
    return waits >= s.minWaits && waits <= s.maxWaits;
}

static bool checkValid(const vector<int> &tiles)
{
    if (tiles.size() != 13)
        return false;
    array<int, 10> c{};
    for (int t : tiles)
    {
        if (t < 1 || t > 9)
            return false;
        if (++c[t] > 4)
            return false;
    }
    return true;
}

static string joined(const vector<int> &v)
{
    string s;
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i)
            s += " ";
        s += to_string(v[i]);
    }
    return s;
}

int main()
{
    static ofstream REPORT("verify_report.txt");
    cout.rdbuf(REPORT.rdbuf());

    buildMeldCombos();
    cout << "reference meld shapes: " << g_meldCombos.size() << "\n\n";
    srand(20260929);

    // ---- Test 1: canWinWith14Tiles vs independent reference (incl. 7 pairs)
    long long t1 = 0, bad1 = 0, sevenOnly = 0;
    for (int iter = 0; iter < 60000; ++iter)
    {
        vector<int> hand;
        array<int, 10> c{};
        for (int i = 0; i < 14; ++i)
        {
            int v;
            do
            {
                v = rand() % 9 + 1;
            } while (c[v] >= 4);
            c[v]++;
            hand.push_back(v);
        }
        const bool prog = canWinWith14Tiles(hand);
        const bool ref = refWin(hand);
        ++t1;
        if (prog != ref)
        {
            if (bad1 < 5)
                cout << "  MISMATCH win: " << joined(hand) << " prog=" << prog
                     << " ref=" << ref << "\n";
            ++bad1;
        }
        if (prog && !refWinStandard(hand) && refWinSevenPairs(hand))
            ++sevenOnly;
    }
    cout << "[Test1] 14-tile win check: " << t1 << " hands, mismatches=" << bad1
         << ", of which 7-pairs-only wins = " << sevenOnly << "\n";
    cout.flush();

    // ---- Test 2: findWaitSet vs independent reference
    long long t2 = 0, bad2 = 0, emptyWaits = 0;
    for (int iter = 0; iter < 60000; ++iter)
    {
        vector<int> hand;
        array<int, 10> c{};
        for (int i = 0; i < 13; ++i)
        {
            int v;
            do
            {
                v = rand() % 9 + 1;
            } while (c[v] >= 4);
            c[v]++;
            hand.push_back(v);
        }
        sort(hand.begin(), hand.end());

        const set<int> got = findWaitSet(hand);
        set<int> ref;
        for (int t = 1; t <= 9; ++t)
        {
            if (c[t] >= 4)
                continue;
            vector<int> cand = hand;
            cand.push_back(t);
            if (refWin(cand))
                ref.insert(t);
        }
        ++t2;
        if (got != ref)
        {
            if (bad2 < 5)
            {
                cout << "  MISMATCH waits: " << joined(hand) << " prog={";
                for (int x : got)
                    cout << x << " ";
                cout << "} ref={";
                for (int x : ref)
                    cout << x << " ";
                cout << "}\n";
            }
            ++bad2;
        }
        if (got.empty())
            ++emptyWaits;
    }
    cout << "[Test2] 13-tile wait sets: " << t2 << " hands, mismatches=" << bad2
         << " (tenpai " << (t2 - emptyWaits) << " / not tenpai " << emptyWaits
         << ")\n";
    cout.flush();

    // ---- Test 3: curated shapes + safety-net hands must be valid and in band
    Difficulty diffs[3] = {Difficulty::Easy, Difficulty::Medium, Difficulty::Hard};
    cout << "\n[Test3] curated template shapes and safety nets:\n";
    long long bad3 = 0;
    for (Difficulty d : diffs)
    {
        const vector<vector<int>> &curated = curatedHandsFor(d);
        for (const vector<int> &h : curated)
        {
            if (!checkValid(h) || !inBand((int)findWaitSet(h).size(), d))
            {
                ++bad3;
                cout << "  BAD curated hand for " << formatDifficulty(d) << ": "
                     << joined(h) << "\n";
            }
        }
        const Exercise &fb = lastResortExercise(d);
        const bool fbOk = checkValid(fb.tiles) && !fb.wait.empty() &&
                          inBand((int)fb.wait.size(), d);
        if (!fbOk)
            ++bad3;
        cout << "  " << formatDifficulty(d) << ": curated=" << curated.size()
             << "  safetyNet waits={";
        for (int x : fb.wait)
            cout << x << " ";
        cout << "} " << (fbOk ? "OK" : "INVALID") << "\n";
    }
    cout << "[Test3] invalid curated/safety-net hands: " << bad3 << "\n";
    cout.flush();

    // ---- Test 4: simulate long sessions with the REAL generateExercise
    cout << "\n[Test4] simulated sessions (every served hand re-validated):\n";
    long long bad4 = 0;
    for (Difficulty d : diffs)
    {
        const string fbKey = handKey(lastResortExercise(d).tiles);
        for (int rounds : {10, 50, 120, 400})
        {
            srand(4242 + rounds);
            set<string> used;
            set<string> distinct;
            int safetyNetServed = 0;
            int cycleResets = 0;

            for (int r = 0; r < rounds; ++r)
            {
                const size_t before = used.size();
                Exercise ex = generateExercise(d, used);
                if (used.size() <= before)
                    ++cycleResets; // pool was exhausted and the cycle restarted
                if (!checkValid(ex.tiles) || ex.wait.empty() ||
                    ex.wait != findWaitSet(ex.tiles) || !inBand((int)ex.wait.size(), d))
                {
                    ++bad4;
                    if (bad4 <= 5)
                        cout << "  BAD served hand (" << formatDifficulty(d)
                             << "): " << joined(ex.tiles) << "\n";
                }
                const string k = handKey(ex.tiles);
                if (k == fbKey)
                    ++safetyNetServed;
                distinct.insert(k);
            }
            cout << "  " << formatDifficulty(d) << " " << rounds
                 << " rounds -> distinct=" << distinct.size()
                 << ", safety-net hand served " << safetyNetServed
                 << " time(s), cycle resets=" << cycleResets << "\n";
        }
    }
    cout << "[Test4] invalid served hands: " << bad4 << "\n";
    cout.flush();

    // ---- Test 5: answer parsing
    cout << "\n[Test5] answer parsing:\n";
    struct Case
    {
        const char *text;
        bool valid;
        const char *expected;
    };
    const vector<Case> cases = {
        // the three required modes: glued, comma separated, space separated
        {"47", true, "4 7"},
        {"4,7", true, "4 7"},
        {"4 7", true, "4 7"},
        // separators are interchangeable / optional / repeatable
        {"4, 7", true, "4 7"},
        {"4,,7", true, "4 7"},
        {"4,7 8", true, "4 7 8"},
        {"4747", true, "4 7"},
        {"9", true, "9"},
        {"7 ", true, "7"},
        {"9 1 4", true, "1 4 9"},
        {"13", true, "1 3"},
        // full-width characters typed with a Chinese IME, in both the GBK
        // (code page 936) and UTF-8 byte forms
        {"4\xA3\xAC" "7", true, "4 7"},              // 4，7 (GBK)
        {"4\xEF\xBC\x8C" "7", true, "4 7"},          // 4，7 (UTF-8)
        {"4\xA1\xA2" "7", true, "4 7"},              // 4、7 (GBK)
        {"4\xE3\x80\x81" "7", true, "4 7"},          // 4、7 (UTF-8)
        {"\xA3\xB4\xA3\xB7", true, "4 7"},           // ４７ (GBK)
        {"\xEF\xBC\x94\xEF\xBC\x97", true, "4 7"},   // ４７ (UTF-8)
        {"2\xA3\xAC" "9", true, "2 9"},              // the real case from the log
        // rejected input
        {"", false, ""},
        {"   ", false, ""},
        {",", false, ""},
        {"0", false, ""},
        {"10", false, ""},
        {"4.5", false, ""},
        {"abc4", false, ""},
        {"-3", false, ""},
        {"5x6", false, ""},
        {"\xEF\xBC\x90", false, ""},                 // ０ full-width zero
        {"\xA3\xB0", false, ""},                     // ０ full-width zero (GBK)
        {"\xC4\xE3", false, ""},                     // 你 (unrelated CJK)
    };
    long long bad5 = 0;
    for (const Case &tc : cases)
    {
        vector<int> out;
        const bool ok = parseWaitValues(tc.text, out);
        const string got = joined(out);
        if (!((ok == tc.valid) && (got == tc.expected)))
        {
            ++bad5;
            cout << "  FAIL input=\"" << tc.text << "\" valid=" << ok
                 << " parsed=\"" << got << "\" (expected valid=" << tc.valid
                 << " \"" << tc.expected << "\")\n";
        }
    }
    cout << "[Test5] parsing failures: " << bad5 << "\n";
    cout.flush();

    // ---- Test 6: the exact grading used by main() for the three input modes
    cout << "\n[Test6] grading accepts \"23\", \"2,3\" and \"2 3\" for a hand "
            "waiting on 2 and 3:\n";
    const vector<int> hand = {1, 2, 2, 3, 4, 5, 6, 7, 7, 7, 9, 9, 9};
    const set<int> waits = findWaitSet(hand);
    string waitsText;
    for (int w : waits)
        waitsText += to_string(w) + " ";
    cout << "  hand " << joined(hand) << " waits on {" << waitsText << "}\n";

    long long bad6 = 0;
    if (waits != set<int>{2, 3})
    {
        ++bad6;
        cout << "  hand does not wait on exactly {2,3} as assumed\n";
    }

    struct GradeCase
    {
        const char *text;
        bool expectCorrect;
    };
    const vector<GradeCase> gradeCases = {
        {"23", true},   {"2,3", true}, {"2 3", true}, {"32", true},
        {"2 3 ", true}, {"2\xA3\xAC" "3", true},        // 2，3 typed with a GBK IME
        {"\xA3\xB2\xA3\xB3", true},                     // ２３ full-width
        {"234", false}, {"2", false},  {"abc", false},
        {"", false},    {"02", false},
    };
    for (const GradeCase &gc : gradeCases)
    {
        vector<int> nums;
        const bool ok = parseWaitValues(gc.text, nums);
        const set<int> userSet(nums.begin(), nums.end());
        const bool correct = ok && (userSet == waits);
        if (correct != gc.expectCorrect)
        {
            ++bad6;
            cout << "  FAIL answer=\"" << gc.text << "\" -> correct=" << correct
                 << " (expected " << gc.expectCorrect << ")\n";
        }
    }
    cout << "[Test6] grading failures: " << bad6 << "\n";
    cout.flush();

    const long long total = bad1 + bad2 + bad3 + bad4 + bad5 + bad6;
    cout << "\n===== TOTAL PROBLEMS FOUND: " << total << " =====\n";
    cout << "DONE\n";
    cout.flush();
    REPORT.flush();
    return 0;
}
