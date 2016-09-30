#include <iostream>
#include <array>
#include <vector>
#include <algorithm>
#include <cmath>
#include <unistd.h>

static std::random_device random_device;

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
    int x, y;
    bool operator()(const Unit& u) const {  return u.x <= x+1 and u.x+1 >= x and u.y <= y+1 and u.y+1 >= y;  }
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
          x0(d.x), y0(d.y),
          dx((j.x-d.x)/std::sqrt(dist2(d,j))),
          dy((j.y-d.y)/std::sqrt(dist2(d,j)))
    {
        type = dx > 0.9 ? '-' : dx > 0.45 ? '/' : dx > -0.45 ? '|' : dx > -0.9 ? '\\' : '-';
    }
};

struct Area
{
    Area(int xL, int xR, int yT, int yB)
        : xL(std::min(xL,xR)), xR(std::max(xL,xR)),
          yT(std::min(yT,yB)), yB(std::max(yT,yB)),
          data((xR-xL+1)*(yB-yT+1), ' ')
    {}

    char operator()(int x, int y) const {  return x < xL or x > xR or y < yT or y > yB ? 0 : get(x,y);  }

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

    void move(const Unit& u, int x, int y)
    {
        auto x0 = std::min(std::max(x,xL), xR);
        auto y0 = std::min(std::max(y,yT), yB);
        //std::printf("move to {%i,%i} : x = %i + lround(%i/%f);  y = %i + lround(%i/%f)\n", x, y, u.x, x-u.x, 
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
                        mobs.emplace_back(1, Unit{ int(x),int(y),type });
                    else
                        m->emplace_back(Unit{ int(x),int(y),type });
                }
        // TODO make convex
        return mobs;
    }

    void add_a_beam(const Unit& src, const Unit& dst)
    {
        beams.emplace_back(src, dst);
    }

    struct BeamOverJedi
    {
        const Unit& jedi;
        bool operator()(const Beam& beam) const { return jedi.x==beam.x and jedi.y==beam.y; }
    };

    void move_beams()
    {
        constexpr unsigned agility_limit = 4;

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
    }

  private:
    const int xL, xR, yT, yB;
    std::vector<char> data;
    std::vector<Beam> beams;

    char get(int x, int y) const {
        auto z = data[y*(xR-xL+1) + x];
        if(z != ' ')
            return z;

        char c = 0;
        for(auto beam : beams)
        {
            if(beam.x != x or beam.y != y)
                continue;
            if(c == '+' and beam.type != '-' and beam.type != '|')
            {
                c = '*';
                break;
            }
            else if(c and c != beam.type)
                c = '+';
            else
                c = beam.type;
        }
        return c ? c : z;
    }
    char& get(int x, int y) {  return data[y*(xR-xL+1) + x];  }

  public:
    void seed()
    {
        std::default_random_engine e(random_device());
        std::uniform_int_distribution<unsigned> uniform_dist(0, 99);

        for(auto y=yT; y<=yB; y++)
            for(auto x=xL; x<=xR; x++)
            {
                auto i = uniform_dist(e);
                if(x*3 < xL+xR and i<=12)
                    get(x,y) = 'D';
                if(x*4 > 3*(xL+xR) and i>=98)
                    get(x,y) = '@';
            }
    }
};

MobCenter mob_center(const Mob& mob)
{
    float x = std::accumulate(mob.begin(), mob.end(), 0, [](size_t a, auto u){ return a + u.x; });
    float y = std::accumulate(mob.begin(), mob.end(), 0, [](size_t a, auto u){ return a + u.y; });
    return MobCenter(  std::lround(x / mob.size()), std::lround(y / mob.size()), mob[0].type, mob.size()  );
}

static MobCenter atk_likeness(const Unit& jedi, const Area& area)
{
    constexpr unsigned w[8] = {5,4,3,1,0,1,3,4};
    constexpr int matrix[8][2] = { {0,1}, {0,1}, {-1,0}, {-1,0}, {0,-1}, {0,-1}, {1,0}, {1,0} };

    MobCenter atk[8] = {};
    for(unsigned j=0; j<8; j++)
    {
        if(j == 0) {
            atk[j].x = jedi.x+1;
            atk[j].y = jedi.y+1;
        } else {
            atk[j].x = atk[j-1].x + matrix[j][0];
            atk[j].y = atk[j-1].y + matrix[j][1];
        }
        atk[j].type = area(atk[j].x, atk[j].y);
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
    auto e = std::remove_if(std::begin(atk), std::end(atk), [&a](auto m){ return m.weigth < a->weigth; });

    std::default_random_engine re(random_device());
    auto i = std::uniform_int_distribution<unsigned>(0, e - atk - 1)(re);
    return atk[i];
}

static bool in_los(const Unit& d, const Unit& jedi, const Area& area)
{
    Beam beam(d, jedi);
    for(unsigned i=1; i<beam.max_age; i++)
    {
        auto x = beam.x0 + std::lround(beam.dx * i);
        auto y = beam.y0 + std::lround(beam.dy * i);
        if(jedi.x == x and jedi.y == y)
            return true;
        if(area(x,y) != ' ')
            return false;
    }
    return false;
}

static bool shoot_at_any_jedi(const Unit& d, const Mobs& jedi_mobs, Area& area)
{
    constexpr unsigned shooting_horizon = 20;
    for(auto jedis : jedi_mobs)
        for(auto jedi : jedis)
            if(dist2(d,jedi) <= shooting_horizon*shooting_horizon and in_los(d,jedi,area))
            {
                area.add_a_beam(d, jedi);
                return false/*true*/;
            }
    return false;
}

bool time_step(Area& area)
{
    constexpr unsigned big_enough = 10;

    // Jedis avoid (or not) blaster beams
    area.move_beams();

    // Jedis move
    const auto jedi_mobs = area.get_mobs( '@');
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
                auto atk_dir = atk_likeness(jedi,area);
                if(atk_dir.weigth)
                    area.move(jedi, atk_dir.x, atk_dir.y);
                else
                {
                    auto m = std::min_element(dcs.begin(), dcs.end(), [&jedi](auto l, auto r){ return dist2(jedi,l) < dist2(jedi,r); });
                    area.move(jedi, m->x, m->y);
                }
            }
    }

    // Droids move
    const auto droids = area.get_mobs('D');
    if(droids.empty())
        return false;

    std::vector<MobCenter> dcs(droids.size());
    for(unsigned i=0; i<dcs.size(); i++)
        dcs[i] = mob_center(droids[i]);

    std::default_random_engine e(random_device());

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
                    {
                        std::uniform_int_distribution<unsigned> uniform_dist(0, dcs.size()-1);
                        auto i = uniform_dist(e);
                        area.move(d, dcs[i].x, dcs[i].y);
                    }
                    else
                    {
                        std::uniform_int_distribution<unsigned> uniform_dist(0, candidates.size()-1);
                        auto i = uniform_dist(e);
                        area.move(d, candidates[i].x, candidates[i].y);
                    }
                }
            }
        }
    }
    return true;
}

int main()
{
    Area area(0,79,0,24);
    area.seed();

    std::printf("\033[2J");
    do
    {
        sleep(1);
        area.out();
    }
    while(time_step(area));
    area.out();

    for(auto m : area.get_mobs('D'))
    {
        auto mc = mob_center(m);
        std::printf("MC :: %.3i %.3i w=%zu\n", mc.x, mc.y, mc.weigth);
    }
}
