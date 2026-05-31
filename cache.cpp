#include <SFML/Graphics.hpp>
#include <optional>
#include <queue>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <string>
#include <algorithm>
#include <cstdio>

const int   CAPACITY = 4;
const float WIN_W    = 1100.f;
const float WIN_H    = 740.f;

// ── Colors ───────────────────────────────────────────────────────────────────
const sf::Color BG          (12,  12,  20);
const sf::Color PANEL       (22,  22,  36);
const sf::Color PANEL2      (18,  18,  30);
const sf::Color DIVIDER     (38,  38,  58);
const sf::Color WHITE       (218, 218, 232);
const sf::Color MUTED       (90,  90, 125);
const sf::Color HIT_COL     (34, 197,  94);
const sf::Color MISS_COL    (239, 68,  68);
const sf::Color EVICT_COL   (251,191,  36);
const sf::Color PUSH_COL    (99, 102, 241);
const sf::Color FIFO_ACCENT (59, 130, 246);
const sf::Color LRU_ACCENT  (168, 85, 247);
const sf::Color SLOT_FILL   (30,  30,  50);
const sf::Color SLOT_EMPTY  (18,  18,  32);
const sf::Color SLOT_OUT    (55,  55,  85);
const sf::Color SLOT_DASH   (42,  42,  65);

// ── Helpers ──────────────────────────────────────────────────────────────────
sf::Color lerpCol(sf::Color a, sf::Color b, float t)
{
    return sf::Color(
        (uint8_t)(a.r + t*(b.r-a.r)),
        (uint8_t)(a.g + t*(b.g-a.g)),
        (uint8_t)(a.b + t*(b.b-a.b)));
}

void drawRect(sf::RenderWindow& w, float x, float y, float wd, float ht,
              sf::Color fill, sf::Color outline = sf::Color::Transparent, float thick = 0.f)
{
    sf::RectangleShape r({wd,ht});
    r.setPosition({x,y});
    r.setFillColor(fill);
    if (thick > 0.f){ r.setOutlineColor(outline); r.setOutlineThickness(thick); }
    w.draw(r);
}

// ── CacheResult ───────────────────────────────────────────────────────────────
struct CacheResult
{
    bool isHit       = false;
    bool wasEviction = false;
    int  evictedVal  = -1;
    int  flashSlot   = -1;
    int  movedSlot   = -1;  // LRU: old index of hit item
};

// ── FIFO ─────────────────────────────────────────────────────────────────────
struct FifoCache
{
    std::queue<int> q;

    CacheResult request(int val)
    {
        CacheResult res;
        std::queue<int> tmp = q;
        int idx = 0;
        while (!tmp.empty())
        {
            if (tmp.front() == val){ res.isHit = true; res.flashSlot = idx; break; }
            tmp.pop(); idx++;
        }
        if (res.isHit) return res;

        res.wasEviction = ((int)q.size() == CAPACITY);
        if (res.wasEviction){ res.evictedVal = q.front(); q.pop(); }
        q.push(val);
        res.flashSlot = (int)q.size()-1;
        return res;
    }

    std::vector<int> snapshot() const
    {
        std::vector<int> v;
        std::queue<int> tmp = q;
        while (!tmp.empty()){ v.push_back(tmp.front()); tmp.pop(); }
        return v;
    }
};

// ── LRU ──────────────────────────────────────────────────────────────────────
struct LruCache
{
    std::vector<int> v;   // index 0 = LRU, back = MRU

    CacheResult request(int val)
    {
        CacheResult res;
        auto it = std::find(v.begin(), v.end(), val);
        if (it != v.end())
        {
            res.isHit     = true;
            res.movedSlot = (int)(it - v.begin());
            v.erase(it);
            v.push_back(val);
            res.flashSlot = (int)v.size()-1;
            return res;
        }
        res.wasEviction = ((int)v.size() == CAPACITY);
        if (res.wasEviction){ res.evictedVal = v.front(); v.erase(v.begin()); }
        v.push_back(val);
        res.flashSlot = (int)v.size()-1;
        return res;
    }

    std::vector<int> snapshot() const { return v; }
};

struct LogEntry { std::string txt; sf::Color col; };

// ── Draw cache row ────────────────────────────────────────────────────────────
void drawCacheRow(sf::RenderWindow& win, sf::Font& font,
                  const std::vector<int>& slots,
                  float rowX, float rowY,
                  float slotW, float slotH, float slotGap,
                  sf::Color accent,
                  const CacheResult& res, float flashT,
                  bool lruMode)
{
    for (int i = 0; i < CAPACITY; i++)
    {
        float sx       = rowX + i*(slotW+slotGap);
        bool  occupied = (i < (int)slots.size());
        bool  flashing = (flashT > 0.f && res.flashSlot == i);

        sf::Color fill    = occupied ? SLOT_FILL : SLOT_EMPTY;
        sf::Color outline = occupied ? SLOT_OUT  : SLOT_DASH;
        float     thick   = 2.f;

        if (flashing)
        {
            sf::Color fc = res.isHit ? HIT_COL : (res.wasEviction ? EVICT_COL : PUSH_COL);
            outline = fc; thick = 3.f;
            fill    = lerpCol(fill, fc, flashT * 0.35f);
        }

        drawRect(win, sx, rowY, slotW, slotH, fill, outline, thick);

        if (occupied)
        {
            sf::Color valCol = flashing
                ? (res.isHit ? HIT_COL : (res.wasEviction ? EVICT_COL : PUSH_COL))
                : WHITE;

            sf::Text vt(font);
            vt.setString(std::to_string(slots[i]));
            vt.setCharacterSize(36);
            vt.setFillColor(valCol);
            vt.setStyle(sf::Text::Bold);
            float tw = vt.getLocalBounds().size.x;
            vt.setPosition({sx + slotW/2.f - tw/2.f, rowY + 20.f});
            win.draw(vt);
        }
        else
        {
            sf::Text et(font);
            et.setString("empty");
            et.setCharacterSize(12);
            et.setFillColor(SLOT_DASH);
            float ew = et.getLocalBounds().size.x;
            et.setPosition({sx + slotW/2.f - ew/2.f, rowY + slotH/2.f - 8.f});
            win.draw(et);
        }

        // sub-label below slot
        std::string subLbl;
        if (lruMode)
            subLbl = (i==0) ? "LRU" : (i==CAPACITY-1 ? "MRU" : "s"+std::to_string(i+1));
        else
            subLbl = (i==0) ? "FRONT" : (i==CAPACITY-1 ? "BACK" : "s"+std::to_string(i+1));

        sf::Text sl(font);
        sl.setString(subLbl);
        sl.setCharacterSize(11);
        bool isEndLabel = (i==0 || i==CAPACITY-1);
        sl.setFillColor(isEndLabel ? accent : MUTED);
        float slw = sl.getLocalBounds().size.x;
        sl.setPosition({sx + slotW/2.f - slw/2.f, rowY + slotH + 5.f});
        win.draw(sl);

        // arrow between slots — plain ASCII ">"  replaced with drawn shape
        if (i < CAPACITY-1)
        {
            float ax = sx + slotW + 4.f;
            float ay = rowY + slotH/2.f;
            drawRect(win, ax, ay-1.f, slotGap-12.f, 2.f, MUTED);
            sf::ConvexShape head; head.setPointCount(3);
            head.setPoint(0,{0.f,-5.f});
            head.setPoint(1,{0.f, 5.f});
            head.setPoint(2,{8.f, 0.f});
            head.setFillColor(MUTED);
            head.setPosition({ax + slotGap-12.f, ay});
            win.draw(head);
        }
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main()
{
    srand((unsigned)time(nullptr));

    // Titlebar + Close only — no resize, no fullscreen
    sf::RenderWindow window(
        sf::VideoMode({(unsigned)WIN_W, (unsigned)WIN_H}),
        "Cache Visualizer  |  FIFO vs LRU",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    sf::Font font;
    if (!font.openFromFile("arial.ttf")) return -1;

    // ── State ────────────────────────────────────────────────────────────────
    FifoCache fifo;
    LruCache  lru;

    int  currentRequest = -1;
    CacheResult fifoRes, lruRes;
    int  fifoHits=0, fifoMisses=0;
    int  lruHits=0,  lruMisses=0;

    float fifoFlash=0.f, lruFlash=0.f;
    sf::Clock clk;

    std::vector<LogEntry> fifoLog, lruLog;

    // ── Layout ────────────────────────────────────────────────────────────────
    const float SLOT_W   = 100.f;
    const float SLOT_H   = 95.f;
    const float SLOT_GAP = 28.f;
    float totalSlotW = CAPACITY*SLOT_W + (CAPACITY-1)*SLOT_GAP;

    float fifoColX = WIN_W*0.5f - totalSlotW - 35.f;
    float lruColX  = WIN_W*0.5f + 35.f;
    float rowY     = 285.f;

    // ── Button ───────────────────────────────────────────────────────────────
    sf::RectangleShape btn({180.f, 48.f});
    btn.setPosition({WIN_W/2.f-90.f, 95.f});

    // ── Text helper ──────────────────────────────────────────────────────────
    auto mkText = [&](const std::string& s, unsigned sz, sf::Color col=WHITE) -> sf::Text
    {
        sf::Text t(font);
        t.setString(s);
        t.setCharacterSize(sz);
        t.setFillColor(col);
        return t;
    };

    auto centreX = [&](sf::Text& t, float cx)
    {
        float tw = t.getLocalBounds().size.x;
        auto  p  = t.getPosition();
        t.setPosition({cx - tw/2.f, p.y});
    };

    // ─────────────────────────────────────────────────────────────────────────
    while (window.isOpen())
    {
        float dt = clk.restart().asSeconds();
        auto decay = [](float& f, float d){ f -= d*1.8f; if(f<0.f)f=0.f; };
        decay(fifoFlash, dt);
        decay(lruFlash,  dt);

        // ── Events ───────────────────────────────────────────────────────────
        while (const std::optional ev = window.pollEvent())
        {
            if (ev->is<sf::Event::Closed>()) window.close();
            if (const auto* k = ev->getIf<sf::Event::KeyPressed>())
                if (k->code == sf::Keyboard::Key::Escape) window.close();
        }

        // ── Hover / click ─────────────────────────────────────────────────────
        auto mp  = sf::Mouse::getPosition(window);
        bool hov = btn.getGlobalBounds().contains(sf::Vector2f(mp));
        btn.setFillColor(hov ? sf::Color(96,165,250) : sf::Color(59,130,246));

        if (sf::Mouse::isButtonPressed(sf::Mouse::Button::Left) && hov
            && fifoFlash <= 0.f && lruFlash <= 0.f)
        {
            currentRequest = rand() % 10;

            fifoRes = fifo.request(currentRequest);
            lruRes  = lru.request(currentRequest);

            if (fifoRes.isHit) fifoHits++;  else fifoMisses++;
            if (lruRes.isHit)  lruHits++;   else lruMisses++;
            fifoFlash = lruFlash = 1.f;

            // build log lines — plain ASCII only
            auto mkLog = [&](const CacheResult& r, int req) -> LogEntry
            {
                if (r.isHit)
                    return {"HIT   pg " + std::to_string(req), HIT_COL};
                if (!r.wasEviction)
                    return {"MISS  pg " + std::to_string(req) + " (free slot)", PUSH_COL};
                return {"EVICT pg " + std::to_string(r.evictedVal)
                        + " -> pg " + std::to_string(req), EVICT_COL};
            };

            fifoLog.push_back(mkLog(fifoRes, currentRequest));
            lruLog.push_back(mkLog(lruRes,   currentRequest));
            if ((int)fifoLog.size() > 10) fifoLog.erase(fifoLog.begin());
            if ((int)lruLog.size()  > 10) lruLog.erase(lruLog.begin());

            sf::sleep(sf::milliseconds(150));
        }

        auto fifoSlots = fifo.snapshot();
        auto lruSlots  = lru.snapshot();

        // ── Draw ─────────────────────────────────────────────────────────────
        window.clear(BG);

        // thin top accent bar
        drawRect(window, 0.f, 0.f, WIN_W, 3.f,
                 lerpCol(FIFO_ACCENT, LRU_ACCENT, 0.5f));

        // ── Title + subtitle ─────────────────────────────────────────────────
        {
            auto t = mkText("Cache Visualizer", 28, WHITE);
            t.setStyle(sf::Text::Bold);
            t.setPosition({0.f, 16.f}); centreX(t, WIN_W/2.f);
            window.draw(t);

            auto s = mkText("capacity = " + std::to_string(CAPACITY)
                            + "   |   pages 0-9", 13, MUTED);
            s.setPosition({0.f, 54.f}); centreX(s, WIN_W/2.f);
            window.draw(s);
        }

        // ── Button ───────────────────────────────────────────────────────────
        window.draw(btn);
        {
            auto bt = mkText("ADD RANDOM", 19, WHITE);
            bt.setStyle(sf::Text::Bold);
            bt.setPosition({0.f, 109.f}); centreX(bt, WIN_W/2.f);
            window.draw(bt);
        }

        // ── Request badge ────────────────────────────────────────────────────
        if (currentRequest != -1)
        {
            sf::Color tc = (fifoRes.isHit && lruRes.isHit)   ? HIT_COL  :
                           (!fifoRes.isHit && !lruRes.isHit)  ? MISS_COL : EVICT_COL;
            auto rt = mkText("Request:  pg " + std::to_string(currentRequest), 20, tc);
            rt.setPosition({0.f, 160.f}); centreX(rt, WIN_W/2.f);
            window.draw(rt);
        }
        else
        {
            auto rt = mkText("Click ADD RANDOM to start", 15, MUTED);
            rt.setPosition({0.f, 163.f}); centreX(rt, WIN_W/2.f);
            window.draw(rt);
        }

        // ── Vertical divider ─────────────────────────────────────────────────
        drawRect(window, WIN_W/2.f-0.5f, 188.f, 1.f, WIN_H-200.f, DIVIDER);

        // ── Column headers ────────────────────────────────────────────────────
        {
            float fifoCX = fifoColX + totalSlotW/2.f;
            float lruCX  = lruColX  + totalSlotW/2.f;

            auto fh = mkText("FIFO", 22, FIFO_ACCENT);
            fh.setStyle(sf::Text::Bold);
            fh.setPosition({0.f,198.f}); centreX(fh, fifoCX); window.draw(fh);
            auto fs = mkText("First In, First Out", 12, MUTED);
            fs.setPosition({0.f,226.f}); centreX(fs, fifoCX); window.draw(fs);

            auto lh = mkText("LRU", 22, LRU_ACCENT);
            lh.setStyle(sf::Text::Bold);
            lh.setPosition({0.f,198.f}); centreX(lh, lruCX); window.draw(lh);
            auto ls = mkText("Least Recently Used", 12, MUTED);
            ls.setPosition({0.f,226.f}); centreX(ls, lruCX); window.draw(ls);
        }

        // ── Cache rows ────────────────────────────────────────────────────────
        drawCacheRow(window, font, fifoSlots, fifoColX, rowY,
                     SLOT_W, SLOT_H, SLOT_GAP, FIFO_ACCENT, fifoRes, fifoFlash, false);
        drawCacheRow(window, font, lruSlots,  lruColX,  rowY,
                     SLOT_W, SLOT_H, SLOT_GAP, LRU_ACCENT,  lruRes,  lruFlash,  true);

        // ── Status bars ───────────────────────────────────────────────────────
        float statY = rowY + SLOT_H + 28.f;
        auto statusPair = [&](const CacheResult& r, int req) -> std::pair<std::string,sf::Color>
        {
            if (req == -1)        return {"Waiting for request...", MUTED};
            if (r.isHit)          return {"HIT  -  pg " + std::to_string(req) + " already in cache", HIT_COL};
            if (!r.wasEviction)   return {"MISS  -  pg " + std::to_string(req) + " loaded (free slot)", PUSH_COL};
            return {"MISS  -  pg " + std::to_string(r.evictedVal)
                    + " evicted,  pg " + std::to_string(req) + " loaded", EVICT_COL};
        };

        {
            auto [fs,fc] = statusPair(fifoRes, currentRequest);
            drawRect(window, fifoColX, statY, totalSlotW, 36.f, PANEL);
            auto st = mkText(fs, 13, fc);
            st.setPosition({fifoColX+10.f, statY+10.f}); window.draw(st);

            auto [ls,lc] = statusPair(lruRes, currentRequest);
            drawRect(window, lruColX, statY, totalSlotW, 36.f, PANEL);
            auto lt = mkText(ls, 13, lc);
            lt.setPosition({lruColX+10.f, statY+10.f}); window.draw(lt);
        }

        // ── Stats panels ─────────────────────────────────────────────────────
        float stpY = statY + 48.f;
        auto drawStats = [&](float x, int h, int m, sf::Color accent)
        {
            int   total = h + m;
            float rate  = total ? (h*100.f/total) : 0.f;
            char  buf[16]; snprintf(buf,sizeof(buf),"%.1f%%",rate);

            drawRect(window, x, stpY, totalSlotW, 52.f, PANEL2);

            // hits
            auto hl = mkText("HITS",   10, MUTED); hl.setPosition({x+12.f, stpY+7.f});  window.draw(hl);
            auto hv = mkText(std::to_string(h), 22, HIT_COL);
            hv.setStyle(sf::Text::Bold); hv.setPosition({x+12.f, stpY+20.f}); window.draw(hv);

            // misses
            auto ml = mkText("MISSES", 10, MUTED); ml.setPosition({x+90.f, stpY+7.f});  window.draw(ml);
            auto mv = mkText(std::to_string(m), 22, MISS_COL);
            mv.setStyle(sf::Text::Bold); mv.setPosition({x+90.f, stpY+20.f}); window.draw(mv);

            // hit rate
            auto rl = mkText("HIT RATE", 10, MUTED); rl.setPosition({x+185.f, stpY+7.f}); window.draw(rl);
            auto rv = mkText(buf, 22, accent);
            rv.setStyle(sf::Text::Bold); rv.setPosition({x+185.f, stpY+20.f}); window.draw(rv);

            // progress bar
            float barW = totalSlotW - 24.f;
            drawRect(window, x+12.f, stpY+46.f, barW,          3.f, DIVIDER);
            if (total > 0)
                drawRect(window, x+12.f, stpY+46.f, barW*rate/100.f, 3.f, accent);
        };

        drawStats(fifoColX, fifoHits, fifoMisses, FIFO_ACCENT);
        drawStats(lruColX,  lruHits,  lruMisses,  LRU_ACCENT);

        // ── Comparison badge (centre, between columns) ────────────────────────
        {
            int total = fifoHits + fifoMisses;
            if (total > 0)
            {
                float fifoRate = fifoHits*100.f/total;
                float lruRate  = lruHits*100.f/total;
                std::string cmp;
                sf::Color   cmpCol;
                if (lruRate > fifoRate)
                { cmp = "LRU is better"; cmpCol = LRU_ACCENT; }
                else if (fifoRate > lruRate)
                { cmp = "FIFO is better"; cmpCol = FIFO_ACCENT; }
                else
                { cmp = "Tied"; cmpCol = MUTED; }

                auto ct = mkText(cmp, 12, cmpCol);
                ct.setPosition({0.f, stpY+18.f}); centreX(ct, WIN_W/2.f);
                window.draw(ct);
            }
        }

        // ── Log panels ────────────────────────────────────────────────────────
        const float LOG_ROW_H  = 22.f;
        const float LOG_HDR_H  = 26.f;
        float logY = stpY + 62.f;
        float logH = WIN_H - logY - 28.f;
        int   maxRows = (int)((logH - LOG_HDR_H) / LOG_ROW_H);
        if (maxRows < 1) maxRows = 1;

        auto drawLog = [&](float x, const std::vector<LogEntry>& log, sf::Color accent)
        {
            drawRect(window, x, logY, totalSlotW, logH, PANEL2);
            auto hdr = mkText("HISTORY", 10, accent);
            hdr.setPosition({x+12.f, logY+8.f}); window.draw(hdr);
            drawRect(window, x+12.f, logY+22.f, totalSlotW-24.f, 0.5f, accent);

            int start = (int)log.size() - maxRows;
            if (start < 0) start = 0;

            for (int i = start; i < (int)log.size(); i++)
            {
                int   row = i - start;
                float ry  = logY + LOG_HDR_H + row * LOG_ROW_H;
                auto  le  = mkText(log[i].txt, 11, log[i].col);
                le.setPosition({x+12.f, ry});
                window.draw(le);
                if (i < (int)log.size()-1)
                    drawRect(window, x+10.f, ry + LOG_ROW_H - 2.f,
                             totalSlotW-20.f, 0.5f, DIVIDER);
            }
        };

        drawLog(fifoColX, fifoLog, FIFO_ACCENT);
        drawLog(lruColX,  lruLog,  LRU_ACCENT);

        // ── Footer ────────────────────────────────────────────────────────────
        {
            auto ft = mkText("ADD RANDOM to request a page   |   ESC to quit", 12, MUTED);
            ft.setPosition({0.f, WIN_H-20.f}); centreX(ft, WIN_W/2.f);
            window.draw(ft);
        }

        window.display();
    }
    return 0;
}