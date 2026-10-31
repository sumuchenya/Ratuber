#include "UI/HistoryPanelModel.h"
#include <iostream>
using namespace Ratuber::UI;
int fails=0;
#define CHECK(x) do{ if(!(x)){ std::cerr<<"FAIL line "<<__LINE__<<": "#x"\n"; ++fails;} }while(0)
static HistoryItem It(uint64_t id,const char* u,const char* r,ItemStatus s,const char* note=""){ HistoryItem i; i.id=id;i.user=u;i.reply=r;i.status=s;i.note=note; return i; }
int main(){
  PanelMetrics m; m.width=300; m.viewportHeight=200; m.lineHeight=20; m.headerHeight=24; m.padding=10; m.cardPadding=8; m.cardGap=6;
  HistoryPanelModel p(m); p.SetMeasure([](char32_t){ return 10.0f; });   // 折行宽度 = 300-20-16=264 => 26 字
  CHECK(p.Cards().empty()&&p.ContentHeight()==20&&p.MaxScroll()==0&&p.QueuedCount()==0);

  // 卡片结构
  p.SetItems({ It(1,"你好","你好呀",ItemStatus::Done), It(2,"在吗","",ItemStatus::Waiting), It(3,"正在说","回复中",ItemStatus::Speaking),
               It(4,"取消的","",ItemStatus::Cancelled), It(5,"跳过的","回复",ItemStatus::Skipped), It(6,"失败的","",ItemStatus::Failed,"AI 服务器：连接失败了很长很长的原因") });
  auto& c=p.Cards(); CHECK(c.size()==6);
  CHECK(!c[0].hasHeader && c[0].button==CardButton::None && c[0].userLines.size()==1 && c[0].replyLines.size()==1);
  CHECK(c[0].height==2*8+20+20);                       // 无标题行：上下内边距 + 用户一行 + AI 一行
  CHECK(c[1].hasHeader && c[1].button==CardButton::Cancel && c[1].chip==U"排队中" && c[1].replyLines.empty());
  CHECK(c[1].height==2*8+24+20);
  CHECK(c[2].button==CardButton::Skip && c[2].chip==U"播放中");
  CHECK(c[3].button==CardButton::None && c[3].hasHeader && c[3].chip==U"已取消");
  CHECK(c[4].chip==U"已跳过");
  CHECK(c[5].chip.size()<=1+1+1+14+1+2 && c[5].chip.rfind(U"失败：",0)==0 && c[5].chip.back()==U'…');
  CHECK(p.QueuedCount()==1);
  CHECK(c[1].top==c[0].top+c[0].height+6);
  CHECK(c[0].top==10);

  // 折行：超过 26 字换行
  { std::string long30; for(int i=0;i<30;++i) long30+="字"; p.SetItems({ It(1,long30.c_str(),"短",ItemStatus::Done) }); CHECK(p.Cards()[0].userLines.size()==2); // "你：" + 30 字 = 33 字
    CHECK(p.Cards()[0].height==2*8+2*20+20); }
  // 用户文字里的换行不会破坏布局
  p.SetItems({ It(1,"a\nb","c\nd",ItemStatus::Done) }); CHECK(p.Cards()[0].userLines.size()==1&&p.Cards()[0].replyLines.size()==1);

  // 按钮位置 + 点击命中
  std::vector<HistoryItem> many; for(int i=1;i<=10;++i) many.push_back(It(i,"问","答",ItemStatus::Done));
  many.push_back(It(11,"排队的","",ItemStatus::Waiting)); many.push_back(It(12,"正在播","回复",ItemStatus::Speaking));
  p.SetItems(many); CHECK(p.MaxScroll()>0 && p.ScrollY()==p.MaxScroll());          // 本来在底部（0==0）=> 自动滚到底
  { auto& cards=p.Cards(); auto& q=cards[10]; auto& s=cards[11];
    float bx=q.buttonRect.x+5, by=q.buttonRect.y+5-p.ScrollY();                       // 视口坐标
    auto h=p.HitTest(bx,by); CHECK(h.kind==CardButton::Cancel&&h.id==11);
    auto h2=p.HitTest(s.buttonRect.x+5,s.buttonRect.y+5-p.ScrollY()); CHECK(h2.kind==CardButton::Skip&&h2.id==12);
    CHECK(p.HitTest(bx-100,by).kind==CardButton::None);                               // 没点在按钮上
    CHECK(p.HitTest(bx,-5).kind==CardButton::None && p.HitTest(bx,500).kind==CardButton::None); // 视口之外
    CHECK(q.buttonRect.x+q.buttonRect.w==300-10-8);                                   // 按钮靠右
  }

  // 滚动：向上翻了就不再自动滚到底
  p.ScrollBy(-100000); CHECK(p.ScrollY()==0);
  float before=p.ScrollY(); many.push_back(It(13,"新的","",ItemStatus::Waiting)); p.SetItems(many); CHECK(p.ScrollY()==before);
  p.ScrollToBottom(); many.push_back(It(14,"更新的","",ItemStatus::Waiting)); p.SetItems(many); CHECK(p.ScrollY()==p.MaxScroll());   // 在底部就跟着走
  p.ScrollBy(100000); CHECK(p.ScrollY()==p.MaxScroll());
  // 相同的内容不重新排版（滚动位置保持）
  p.ScrollBy(-30); float s0=p.ScrollY(); p.SetItems(many); CHECK(p.ScrollY()==s0);
  // 内容变少时滚动位置被夹住
  p.SetItems({ It(1,"a","b",ItemStatus::Done) }); CHECK(p.ScrollY()==0&&p.MaxScroll()==0);
  // 视口高度变化
  p.SetItems(many); p.SetViewportHeight(100000); CHECK(p.MaxScroll()==0&&p.ScrollY()==0);

  // 展开动画
  { HistoryPanelModel q(m); CHECK(!q.IsOpen()&&q.Progress()==0&&q.SlideOffset()==300);
    q.Toggle(); CHECK(q.IsOpen()); q.Update(0.09); CHECK(q.Progress()>0.4&&q.Progress()<0.6&&q.SlideOffset()>0&&q.SlideOffset()<300);
    q.Update(1.0); CHECK(q.Progress()==1&&q.SlideOffset()==0);
    q.Toggle(); q.Update(1.0); CHECK(q.Progress()==0&&q.SlideOffset()==300); }
  std::cerr<<(fails?"FAILED ":"ALL PASSED ")<<fails<<"\n"; return fails?1:0;
}
