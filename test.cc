#include <iostream>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <unistd.h>

template <typename I, typename RandomEngine> I select_random(I b, I e, RandomEngine& re)
{
    auto z = std::distance(b, e);
    if(z < 2)
        return b;
    std::uniform_int_distribution<unsigned> uniform_dist(0, z-1);
    std::advance(b, uniform_dist(re));
    return b;
}

template <typename C, typename RandomEngine> typename C::const_iterator select_random(const C& c, RandomEngine& re)
{
    auto z = c.size();
    if(z < 2)
        return c.begin();
    std::uniform_int_distribution<unsigned> uniform_dist(0, z-1);
    auto i = c.begin();
    std::advance(i, uniform_dist(re));
    return i;
}

static std::random_device random_device;
static std::default_random_engine re;

struct Unit {  int x, y;  char type;  };

typedef std::vector<Unit> Mob;
typedef std::vector<Mob> Mobs;

struct MobCenter : public Unit
{
    size_t weigth;
    MobCenter(int x, int y, char t, size_t w) : Unit{x,y,t}, weigth(w) {}
    MobCenter() : MobCenter{0,0,0,0} {}
};

struct Near {
    int x, y, d=1;
    bool operator()(const Unit& u) const {  return u.x <= x+d and u.x+d >= x and u.y <= y+d and u.y+d >= y;  }
};

static size_t dist2(const Unit& l, int x, int y) {  return (l.x-x)*(l.x-x) + (l.y-y)*(l.y-y);  }

static size_t dist2(const Unit& l, const Unit& r) {  return dist2(l,r.x,r.y);  }

struct Beam : public Unit
{
    static constexpr unsigned max_age = 30;
    unsigned age = 0;
    int x0, y0;
    float dx, dy;
    Beam(const Unit& d, const Unit& j)
        : Unit(d),
          x0(x), y0(y),
          dx((j.x-x)/std::sqrt(dist2(d,j))),
          dy((j.y-y)/std::sqrt(dist2(d,j)))
    {
        type = dx > 0.9 ? '-' : dx > 0.45 ? dy < 0 ? '/' : '\\' : dx > -0.45 ? '|' : dx > -0.9 ? dy > 0 ? '/' : '\\' : '-';
    }
};

MobCenter mob_center(const Mob& mob)
{
    float x = std::accumulate(mob.begin(), mob.end(), 0, [](size_t a, auto u){ return a + u.x; });
    float y = std::accumulate(mob.begin(), mob.end(), 0, [](size_t a, auto u){ return a + u.y; });
    return MobCenter(  std::lround(x / mob.size()), std::lround(y / mob.size()), mob[0].type, mob.size()  );
}

struct Area
{
    Area(int xL, int xR, int yT, int yB)
        : xL(std::min(xL,xR)), xR(std::max(xL,xR)),
          yT(std::min(yT,yB)), yB(std::max(yT,yB)),
          data((xR-xL+1)*(yB-yT+1), ' ')
    {}

    void out() const
    {
        std::printf("\033[%iA", yB-yT+1);
        for(auto y=yT; y<=yB; y++)
        {
            for(auto x=xL; x<=xR; x++)
                out(get(x,y));
            out('\n');
        }
    }

    static void out(char c)
    {
        // 31 - red; 33 - yellow; 34 - blue; 35 - magenta
        switch(c) {
            case '|':
            case '/':
            case '\\':
            case '-':
            case '+':
            case '*':
            case '#':
                std::printf("\x1b[0;31m%c", c);
                return;
            case 'D':
                std::printf("\x1b[0;33m%c", c);
                return;
            case '@':
                std::printf("\x1b[0;34m%c", c);
                return;
            default:
                std::printf("\x1b[m%c", c);
                return;
        }
    }

    template <typename T> void move_to_any(const Unit& u, const std::vector<T>& c)
    {
        auto i = select_random(c, re);
        move(u, i->x, i->y);
    }

    void move(const Unit& u, int x, int y)
    {
        auto x0 = std::min(std::max(x,xL), xR);
        auto y0 = std::min(std::max(y,yT), yB);

        x = u.x + std::lround((x0-u.x)/std::sqrt(dist2(u,x0,y0)));
        y = u.y + std::lround((y0-u.y)/std::sqrt(dist2(u,x0,y0)));

        if(u.type == get(x,y))
            return;
        if(u.type == 'D' and get(x,y) == '@')
            return;

        get(x,y) = u.type;
        get(u.x,u.y) = ' ';
    }

    Mobs get_mobs(char type) const
    {
        Mobs mobs;
        for(auto y=yT; y<=yB; y++)
            for(auto x=xL; x<=xR; x++)
                if(get(x,y) == type)
                {
                    auto m = std::find_if(mobs.begin(), mobs.end(), [x,y](auto mob){  return std::any_of(mob.begin(), mob.end(), Near{x,y});  });
                    if(m == mobs.end())
                        mobs.emplace_back(1, Unit{x,y,type});
                    else
                        m->emplace_back(Unit{x,y,type});
                }
        // TODO make convex
        return mobs;
    }

    void add_a_beam(const Unit& src, const Unit& dst)
    {
        beams.emplace_back(src, dst);
    }

    void move_beams()
    {
        constexpr unsigned agility_limit = 4;

        for(auto beam : beams)
        {
            char& c = get(beam.x, beam.y);
            switch(c)
            {
                case '|':
                case '/':
                case '\\':
                case '-':
                case '+':
                case '*':
                case '#':
                    c = ' ';
                    break;
            }
        }

        auto be = std::remove_if(beams.begin(), beams.end(), [](auto b){ return b.age >= b.max_age; });
        for(auto beam=beams.begin(); beam!=be; )
        {
            beam->age++;
            beam->x = beam->x0 + std::lround(beam->dx * beam->age);
            beam->y = beam->y0 + std::lround(beam->dy * beam->age);
            if(beam->x < xL or beam->x > xR or beam->y < yT or beam->y > yB)
            {
                std::iter_swap(beam, be-1);
                be --;
            }
            else
                ++beam;
        }
        for(auto jedis : get_mobs('@'))
            for(auto jedi : jedis)
                if(std::count_if(beams.begin(), be, BeamOverJedi{jedi}) > agility_limit)
                {
                    get(jedi.x,jedi.y) = ' ';
                    be = std::remove_if(beams.begin(), be, BeamOverJedi{jedi});
                }
        beams.erase(be, beams.end());

        for(auto beam : beams)
        {
            char& c = get(beam.x, beam.y);
            switch(c)
            {
                case ' ':
                    c = beam.type;
                    break;
                case '|':
                case '/':
                case '\\':
                case '-':
                    c = '+';
                    break;
                case '+':
                    c = '*';
                    break;
                case '*':
                    c = '#';
                    break;
            }
        }
    }

    MobCenter atk_likeness(const Unit& jedi) const
    {
        constexpr unsigned w[8] = {5,3,2,1,0,1,2,3};
        constexpr int matrix[7/*8*/][2] = { {0,1}, {0,1}, {-1,0}, {-1,0}, {0,-1}, {0,-1}, {1,0}/*, {1,0}*/ };

        MobCenter atk[8] = {};
        for(unsigned j=0; j<8; j++)
        {
            if(j == 0) {
                atk[j].x = jedi.x+1;
                atk[j].y = jedi.y-1;
            } else {
                atk[j].x = atk[j-1].x + matrix[j-1][0];
                atk[j].y = atk[j-1].y + matrix[j-1][1];
            }
            atk[j].type = atk[j].x < xL or atk[j].x > xR or atk[j].y < yT or atk[j].y > yB ? 0 : get(atk[j].x, atk[j].y);
        }
        for(unsigned j=0; j<8; j++)
        {
            for(unsigned i=0; i<8; i++)
            {
                auto k = (j + i) % 8;
                if(atk[k].type == 'D')
                    atk[j].weigth += w[i];
            }
        }
        auto a = std::max_element(std::begin(atk), std::end(atk), [](auto l, auto r){ return l.weigth < r.weigth; });
        auto e = std::remove_if(std::begin(atk), std::end(atk), [a](auto m){ return m.weigth < a->weigth; });
        return *select_random(std::begin(atk), e, re);
    }

    bool in_los(const Unit& d, const Unit& jedi) const
    {
        Beam beam(d, jedi);
        for(unsigned i=1; i<beam.max_age; i++)
        {
            auto x = beam.x0 + std::lround(beam.dx * i);
            auto y = beam.y0 + std::lround(beam.dy * i);
            if(jedi.x == x and jedi.y == y)
                return true;
            if(get(x,y) == 'D')
                return false;
        }
        return false;
    }

  protected:

    const int xL, xR, yT, yB;

    char get(int x, int y) const {  return data[y*(xR-xL+1) + x];  }
    char& get(int x, int y) {  return data[y*(xR-xL+1) + x];  }

  private:

    struct BeamOverJedi
    {
        const Unit& jedi;
        bool operator()(const Beam& beam) const { return jedi.x==beam.x and jedi.y==beam.y; }
    };

    std::vector<char> data;
    std::vector<Beam> beams;
};

static bool shoot_at_any_jedi(const Unit& d, const Mobs& jedi_mobs, Area& area)
{
    constexpr unsigned shooting_horizon = 20;
    Mob candidates;
    for(auto jedis : jedi_mobs)
        std::copy_if(jedis.begin(), jedis.end(), std::back_inserter(candidates),
                     [&](auto jedi){ return dist2(d,jedi) <= shooting_horizon*shooting_horizon and area.in_los(d,jedi); });

    if(candidates.empty())
        return false;

    area.add_a_beam(d, *select_random(candidates, re));
    return true;
}

bool time_step(Area& area)
{
    constexpr unsigned big_enough = 10;

    // Jedis avoid (or not) blaster beams
    area.move_beams();

    // Jedis move
    const auto jedi_mobs = area.get_mobs('@');
    if(jedi_mobs.empty())
        return false;
    {
        const auto droids = area.get_mobs('D');
        if(droids.empty())
            return false;

        std::vector<MobCenter> dcs(droids.size());
        for(unsigned i=0; i<dcs.size(); i++)
            dcs[i] = mob_center(droids[i]);

        for(auto jedis : jedi_mobs)
            for(auto jedi : jedis)
            {
                auto atk_dir = area.atk_likeness(jedi);
                if(atk_dir.weigth)
                {
                    area.move(jedi, atk_dir.x, atk_dir.y);
                    continue;
                }

                constexpr unsigned near = 2;
                auto m = std::min_element(dcs.begin(), dcs.end(), [&jedi](auto l, auto r){ return dist2(jedi,l) < dist2(jedi,r); });

                Mob nearest;
                if(dist2(jedi,*m) > near*near)
                    for(auto mob : droids)
                        std::copy_if(mob.begin(), mob.end(), std::back_inserter(nearest), Near{jedi.x,jedi.y,near});
                if(not nearest.empty())
                    area.move_to_any(jedi, nearest);
                else
                    area.move(jedi, m->x, m->y);
            }
    }

    // Droids move
    const auto droids = area.get_mobs('D');
    if(droids.empty())
        return false;

    std::vector<MobCenter> dcs(droids.size());
    for(unsigned i=0; i<dcs.size(); i++)
        dcs[i] = mob_center(droids[i]);

    for(size_t di=0; di<droids.size(); di++)
    {
        for(auto d : droids[di])
        {
            if(not shoot_at_any_jedi(d,jedi_mobs,area))
            {
                auto mc = dcs[di];
                if(mc.weigth < big_enough)
                {
                    std::vector<MobCenter> candidates;
                    std::copy_if(dcs.begin(), dcs.end(), std::back_inserter(candidates), [&mc](auto& c){ return &c != &mc and c.weigth >= big_enough; });
                    if(candidates.empty())
                        area.move_to_any(d, dcs);
                    else
                        area.move_to_any(d, candidates);
                }
            }
        }
    }
    return true;
}

struct RandomArea : public Area
{
    RandomArea(int xL, int xR, int yT, int yB) : Area(xL,xR,yT,yB)
    {
        std::uniform_int_distribution<unsigned> uniform_dist(0, 99);

        for(auto y=yT; y<=yB; y++)
            for(auto x=xL; x<=xR; x++)
            {
                auto i = uniform_dist(::re);
                if(x*3 < xL+xR and i<=12)
                    get(x,y) = 'D';
                if(x*4 > 3*(xL+xR) and i>98)
                    get(x,y) = '@';
            }
    }
};

struct TestArea :  public Area
{
    TestArea() : Area(0,5,0,3)
    {
        get(1,1) = 'D';
        get(2,1) = 'D';
        get(3,1) = 'D';
        get(2,2) = '@';
    }
};

int main()
{
    ::re = std::default_random_engine(random_device());

    RandomArea area(0,78,0,23);
    //RandomArea area(0,138,0,53);
    //TestArea area;

    std::printf("\033[2J");
    do
    {
        sleep(1);
        area.out();
    }
    while(time_step(area));
    area.out();
    return 0;
}

