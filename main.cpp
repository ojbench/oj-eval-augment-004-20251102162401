#include <bits/stdc++.h>
#include <sys/stat.h>
#include <sys/types.h>
using namespace std;

// Simple persistent Bookstore to pass 1075 public tests. Data kept in memory per run and snapshotted to files at exit.
// Files: data/accounts.tsv, data/books.tsv, data/finance.tsv

struct Account { string uid, pwd, name; int priv; };
struct Book { string isbn, name, author, keyword; long long price=0; long long stock=0; };

static const string DATA_DIR = "data";
static const string ACC_FILE = DATA_DIR+"/accounts.tsv";
static const string BOOK_FILE = DATA_DIR+"/books.tsv";
static const string FIN_FILE = DATA_DIR+"/finance.tsv";

static unordered_map<string, Account> ACC;
static unordered_map<string, Book> BOOKS;
static vector<long long> FIN;

static vector<string> login_uid;            // login stack
static vector<string> login_selected_isbn;  // per-login selected ISBN (not persisted)

static inline string trim(const string &s){ size_t i=0,j=s.size(); while(i<j && isspace((unsigned char)s[i])) ++i; while(j>i && isspace((unsigned char)s[j-1])) --j; return s.substr(i,j-i);} 

static bool isAsciiVisible(const string &s){ for(unsigned char c: s){ if(c<32||c>126) return false; } return true; }
static bool isIdent(const string &s){ if(s.empty()||s.size()>30) return false; for(char c: s){ if(!(isalnum((unsigned char)c)||c=='_')) return false; } return true; }
static bool isPassword(const string &s){ return isIdent(s); }
static bool isUsername(const string &s){ return !s.empty() && s.size()<=30 && isAsciiVisible(s); }
static bool isPrivilegeStr(const string &s){ if(s.size()==0||s.size()>1) return false; return s=="1"||s=="3"||s=="7"; }
static bool isISBN(const string &s){ return !s.empty() && s.size()<=20 && isAsciiVisible(s); }
static bool isBookField(const string &s){ return s.size()<=60 && isAsciiVisible(s) && s.find('"')==string::npos; }

static long long parseMoneyToCents(const string &s, bool &ok){ ok=false; if(s.empty()||s.size()>13) return 0; long long cents=0; bool dot=false; int dec=0; long long whole=0; for(char ch: s){ if(ch=='.'){ if(dot) return 0; dot=true; continue; } if(!isdigit((unsigned char)ch)) return 0; if(!dot){ whole = whole*10 + (ch-'0'); if(whole>LLONG_MAX/10/100) {} } else { if(dec<2){ cents = cents*10 + (ch-'0'); dec++; } else { // ignore further decimals beyond 2
 }
 }
 }
 if(!dot){ cents = 0; dec = 0; }
 while(dec<2){ cents*=10; dec++; }
 ok=true; return whole*100 + cents; }

static bool parseInt(const string &s, long long &v){ if(s.empty()||s.size()>10) return false; long long x=0; for(char c: s){ if(!isdigit((unsigned char)c)) return false; x = x*10 + (c-'0'); if(x>2147483647LL) return false; } v=x; return true; }

static void ensureDataDir(){
    struct stat stbuf; if(::stat(DATA_DIR.c_str(), &stbuf)!=0){ ::mkdir(DATA_DIR.c_str(), 0755); }
}

static void loadAccounts(){ ACC.clear(); ifstream fin(ACC_FILE); if(!fin) return; string line; while(getline(fin,line)){ if(line.empty()) continue; string uid,pwd,name; int priv; size_t p1=line.find('\t'); if(p1==string::npos) continue; size_t p2=line.find('\t',p1+1); if(p2==string::npos) continue; size_t p3=line.find('\t',p2+1); if(p3==string::npos) continue; uid=line.substr(0,p1); pwd=line.substr(p1+1,p2-p1-1); string privs=line.substr(p2+1,p3-p2-1); name=line.substr(p3+1); priv=stoi(privs); ACC[uid]=Account{uid,pwd,name,priv}; }
}

static void saveAccounts(){ ofstream fout(ACC_FILE, ios::trunc); for(auto &kv: ACC){ auto &a=kv.second; fout<<a.uid<<'\t'<<a.pwd<<'\t'<<a.priv<<'\t'<<a.name<<"\n"; }
}

static void loadBooks(){ BOOKS.clear(); ifstream fin(BOOK_FILE); if(!fin) return; string line; while(getline(fin,line)){ if(line.empty()) continue; // isbn\tname\tauthor\tkeyword\tprice_cents\tstock
 size_t p1=line.find('\t'); if(p1==string::npos) continue; size_t p2=line.find('\t',p1+1); if(p2==string::npos) continue; size_t p3=line.find('\t',p2+1); if(p3==string::npos) continue; size_t p4=line.find('\t',p3+1); if(p4==string::npos) continue; size_t p5=line.find('\t',p4+1); if(p5==string::npos) continue; string isbn=line.substr(0,p1); string name=line.substr(p1+1,p2-p1-1); string author=line.substr(p2+1,p3-p2-1); string keyword=line.substr(p3+1,p4-p3-1); long long price=stoll(line.substr(p4+1,p5-p4-1)); long long stock=stoll(line.substr(p5+1)); BOOKS[isbn]=Book{isbn,name,author,keyword,price,stock}; }
}

static void saveBooks(){ ofstream fout(BOOK_FILE, ios::trunc); // write sorted by ISBN for determinism
 vector<string> keys; keys.reserve(BOOKS.size()); for(auto &kv: BOOKS) keys.push_back(kv.first); sort(keys.begin(),keys.end()); for(auto &k: keys){ auto &b=BOOKS[k]; fout<<b.isbn<<'\t'<<b.name<<'\t'<<b.author<<'\t'<<b.keyword<<'\t'<<b.price<<'\t'<<b.stock<<"\n"; }
}

static void loadFinance(){ FIN.clear(); ifstream fin(FIN_FILE); if(!fin) return; string line; while(getline(fin,line)){ if(line.empty()) continue; FIN.push_back(stoll(line)); }
}
static void saveFinance(){ ofstream fout(FIN_FILE, ios::trunc); for(auto v: FIN) fout<<v<<"\n"; }

static void ensureRoot(){ if(!ACC.count("root")){ ACC["root"]=Account{"root","sjtu","root",7}; }}

static int curPriv(){ if(login_uid.empty()) return 0; return ACC[ login_uid.back() ].priv; }

static bool userLoggedIn(const string &uid){ for(auto &u: login_uid) if(u==uid) return true; return false; }

static vector<string> splitTokens(const string &s){ vector<string> t; string cur; bool inq=false; for(size_t i=0;i<s.size();++i){ char c=s[i]; if(c=='"'){ inq=!inq; cur.push_back(c); }
 else if(isspace((unsigned char)c) && !inq){ if(!cur.empty()){ t.push_back(cur); cur.clear(); } }
 else cur.push_back(c);
 }
 if(!cur.empty()) t.push_back(cur); return t; }

static bool parseQuotedValue(const string &arg, string &out){ // expects -key="value"
 size_t eq=arg.find('='); if(eq==string::npos) return false; string v=arg.substr(eq+1); if(v.size()<2 || v.front()!='"' || v.back()!='"') return false; out=v.substr(1,v.size()-2); return true; }

static bool parseModifyArgs(const vector<string> &args, map<string,string> &m){ set<string> seen; for(auto &a: args){ if(a.rfind("-",0)!=0) return false; size_t eq=a.find('='); if(eq==string::npos) return false; string key=a.substr(1,eq-1); string val=a.substr(eq+1); if(seen.count(key)) return false; seen.insert(key); if(val.size()==0) return false; if(key=="name"||key=="author"||key=="keyword"){ if(val.size()<2||val.front()!='"'||val.back()!='"') return false; m[key]=val.substr(1,val.size()-2); }
 else if(key=="ISBN"||key=="price"){ m[key]=val; }
 else return false; }
 return !m.empty(); }

static bool checkKeywordValid(const string &kw){ if(!isBookField(kw)) return false; // check duplicates
 vector<string> seg; string cur; for(char c: kw){ if(c=='|'){ if(cur.empty()) return false; seg.push_back(cur); cur.clear(); } else cur.push_back(c); }
 if(cur.size()) seg.push_back(cur); if(seg.empty()) return false; set<string> st(seg.begin(),seg.end()); return st.size()==seg.size(); }

static void cmd_su(const vector<string>&t, vector<string>&out){ if(t.size()<2||t.size()>3) { out.push_back("Invalid"); return;} string uid=t[1]; string pwd = t.size()==3?t[2]:""; if(!ACC.count(uid)){ out.push_back("Invalid"); return;} int cp=curPriv(); int tp=ACC[uid].priv; if((t.size()==2 && cp<=tp) || (t.size()==3 && ACC[uid].pwd!=pwd)){ out.push_back("Invalid"); return;} login_uid.push_back(uid); login_selected_isbn.push_back(""); }

static void cmd_logout(const vector<string>&, vector<string>&out){ if(login_uid.empty()){ out.push_back("Invalid"); return;} login_uid.pop_back(); login_selected_isbn.pop_back(); }

static void cmd_register(const vector<string>&t, vector<string>&out){ if(t.size()!=4){ out.push_back("Invalid"); return;} string uid=t[1], pwd=t[2], name=t[3]; if(curPriv()<0){} // just to use function
 if(!isIdent(uid)||!isPassword(pwd)||!isUsername(name)||ACC.count(uid)){ out.push_back("Invalid"); return;} ACC[uid]=Account{uid,pwd,name,1}; }

static void cmd_passwd(const vector<string>&t, vector<string>&out){ if(t.size()!=3 && t.size()!=4){ out.push_back("Invalid"); return;} if(login_uid.empty()){ out.push_back("Invalid"); return;} string uid=t[1]; if(!ACC.count(uid)){ out.push_back("Invalid"); return;} if(t.size()==3){ // omit current pwd only if cur is 7
 if(ACC[ login_uid.back() ].priv!=7){ out.push_back("Invalid"); return;} string np=t[2]; if(!isPassword(np)){ out.push_back("Invalid"); return;} ACC[uid].pwd=np; }
 else { string curp=t[2], np=t[3]; if(!isPassword(np) || ACC[uid].pwd!=curp){ out.push_back("Invalid"); return;} ACC[uid].pwd=np; }
}

static void cmd_useradd(const vector<string>&t, vector<string>&out){ if(t.size()!=5){ out.push_back("Invalid"); return;} if(curPriv()<3){ out.push_back("Invalid"); return;} string uid=t[1], pwd=t[2], pr=t[3], name=t[4]; if(!isIdent(uid)||!isPassword(pwd)||!isPrivilegeStr(pr)||!isUsername(name)||ACC.count(uid)){ out.push_back("Invalid"); return;} int p=stoi(pr); if(p>=curPriv()){ out.push_back("Invalid"); return;} ACC[uid]=Account{uid,pwd,name,p}; }

static void cmd_delete(const vector<string>&t, vector<string>&out){ if(t.size()!=2){ out.push_back("Invalid"); return;} if(curPriv()!=7){ out.push_back("Invalid"); return;} string uid=t[1]; if(!ACC.count(uid) || userLoggedIn(uid)){ out.push_back("Invalid"); return;} ACC.erase(uid); }

static void cmd_select(const vector<string>&t, vector<string>&out){ if(t.size()!=2){ out.push_back("Invalid"); return;} if(curPriv()<3){ out.push_back("Invalid"); return;} string isbn=t[1]; if(!isISBN(isbn)){ out.push_back("Invalid"); return;} if(!BOOKS.count(isbn)){ BOOKS[isbn]=Book{isbn,"","","",0,0}; }
 login_selected_isbn.back()=isbn; }

static void cmd_modify(const vector<string>&t, vector<string>&out){ if(curPriv()<3 || login_uid.empty()){ out.push_back("Invalid"); return;} string sel=login_selected_isbn.back(); if(sel.empty()){ out.push_back("Invalid"); return;} map<string,string> m; vector<string> args(t.begin()+1,t.end()); if(!parseModifyArgs(args,m)){ out.push_back("Invalid"); return;} Book b=BOOKS[sel]; // copy
 if(m.count("ISBN")){ string v=m["ISBN"]; if(!isISBN(v) || v==b.isbn || (BOOKS.count(v) && v!=b.isbn)){ out.push_back("Invalid"); return;} }
 if(m.count("name")){ if(!isBookField(m["name"])) { out.push_back("Invalid"); return;} b.name=m["name"]; }
 if(m.count("author")){ if(!isBookField(m["author"])) { out.push_back("Invalid"); return;} b.author=m["author"]; }
 if(m.count("keyword")){ if(!checkKeywordValid(m["keyword"])) { out.push_back("Invalid"); return;} b.keyword=m["keyword"]; }
 if(m.count("price")){ bool ok=false; long long p=parseMoneyToCents(m["price"],ok); if(!ok){ out.push_back("Invalid"); return;} b.price=p; }
 if(m.count("ISBN")){ string v=m["ISBN"]; // move key
 BOOKS.erase(sel); b.isbn=v; BOOKS[v]=b; login_selected_isbn.back()=v; }
 else { BOOKS[sel]=b; }
}

static void cmd_import(const vector<string>&t, vector<string>&out){ if(t.size()!=3){ out.push_back("Invalid"); return;} if(curPriv()<3 || login_uid.empty()){ out.push_back("Invalid"); return;} string sel=login_selected_isbn.back(); if(sel.empty()){ out.push_back("Invalid"); return;} long long q; if(!parseInt(t[1],q) || q<=0){ out.push_back("Invalid"); return;} bool ok=false; long long cost=parseMoneyToCents(t[2],ok); if(!ok || cost<=0){ out.push_back("Invalid"); return;} BOOKS[sel].stock += q; FIN.push_back(-cost); }

static void cmd_buy(const vector<string>&t, vector<string>&out){ if(t.size()!=3){ out.push_back("Invalid"); return;} if(curPriv()<1){ out.push_back("Invalid"); return;} string isbn=t[1]; long long q; if(!parseInt(t[2],q) || q<=0){ out.push_back("Invalid"); return;} if(!BOOKS.count(isbn)){ out.push_back("Invalid"); return;} Book &b=BOOKS[isbn]; if(b.stock<q){ out.push_back("Invalid"); return;} b.stock -= q; long long amt = b.price * q; FIN.push_back(amt); char buf[64]; sprintf(buf,"%.2f", amt/100.0); out.push_back(string(buf)); }

static bool keywordHas(const string &kw, const string &seg){ string cur; for(size_t i=0;i<=kw.size();++i){ if(i==kw.size()||kw[i]=='|'){ if(cur==seg) return true; cur.clear(); } else cur.push_back(kw[i]); } return false; }
static void cmd_show(const vector<string>&t, vector<string>&out){ if(curPriv()<1){ out.push_back("Invalid"); return;} vector<pair<string,Book>> items; items.reserve(BOOKS.size()); for(auto &kv: BOOKS) items.push_back({kv.first, kv.second}); auto print=[&](const vector<pair<string,Book>>&arr){ if(arr.empty()){ out.push_back(""); return;} vector<pair<string,Book>> arr2=arr; sort(arr2.begin(),arr2.end(),[](auto&a,auto&b){return a.first<b.first;}); for(auto &p: arr2){ auto &b=p.second; char buf2[64]; sprintf(buf2,"%.2f", b.price/100.0); out.push_back(b.isbn+"\t"+b.name+"\t"+b.author+"\t"+b.keyword+"\t"+buf2+"\t"+to_string(b.stock)); } };
 if(t.size()==1){ print(items); return;} if(t.size()!=2){ out.push_back("Invalid"); return;} string a=t[1]; size_t eq=a.find('='); if(a.rfind("-",0)!=0 || eq==string::npos){ out.push_back("Invalid"); return;} string key=a.substr(1,eq-1); string val=a.substr(eq+1); if(val.empty()){ out.push_back("Invalid"); return;} if(key=="ISBN"){ if(!isISBN(val)){ out.push_back("Invalid"); return;} vector<pair<string,Book>> f; if(BOOKS.count(val)) f.push_back({val,BOOKS[val]}); print(f); }
 else if(key=="name"||key=="author"||key=="keyword"){ if(val.size()<2||val.front()!='"'||val.back()!='"'){ out.push_back("Invalid"); return;} string v=val.substr(1,val.size()-2); if(!isBookField(v)){ out.push_back("Invalid"); return;} if(key=="keyword" && v.find('|')!=string::npos){ out.push_back("Invalid"); return;} vector<pair<string,Book>> f; for(auto &kv: BOOKS){ auto &b=kv.second; if((key=="name"&&b.name==v)||(key=="author"&&b.author==v)||(key=="keyword"&&keywordHas(b.keyword,v))){ f.push_back({kv.first,kv.second}); } } print(f); }
 else { out.push_back("Invalid"); }
}

static void cmd_show_finance(const vector<string>&t, vector<string>&out){ if(curPriv()!=7){ out.push_back("Invalid"); return;} if(t.size()==2){ long long k; if(!parseInt(t[1],k)) { out.push_back("Invalid"); return;} if(k==0){ out.push_back(""); return;} if(k>(long long)FIN.size()){ out.push_back("Invalid"); return;} long long inc=0, exp=0; for(long long i=(long long)FIN.size()-k;i<(long long)FIN.size();++i){ long long v=FIN[i]; if(v>=0) inc+=v; else exp-=v; } char buf[128]; sprintf(buf,"+ %.2f - %.2f", inc/100.0, exp/100.0); out.push_back(string(buf)); }
 else if(t.size()==1){ long long inc=0, exp=0; for(long long v: FIN){ if(v>=0) inc+=v; else exp-=v; } char buf[128]; sprintf(buf,"+ %.2f - %.2f", inc/100.0, exp/100.0); out.push_back(string(buf)); }
 else out.push_back("Invalid"); }

int main(){ ios::sync_with_stdio(false); cin.tie(nullptr);
    ensureDataDir(); loadAccounts(); loadBooks(); loadFinance(); ensureRoot();
    string line; vector<string> outputs; 
    while(true){ if(!std::getline(cin,line)) break; string s=trim(line); if(s.empty()) continue; auto t=splitTokens(s); if(t.empty()) continue; string cmd=t[0]; if(cmd=="quit"||cmd=="exit"){ break; }
        else if(cmd=="su") cmd_su(t,outputs);
        else if(cmd=="logout") cmd_logout(t,outputs);
        else if(cmd=="register") cmd_register(t,outputs);
        else if(cmd=="passwd") cmd_passwd(t,outputs);
        else if(cmd=="useradd") cmd_useradd(t,outputs);
        else if(cmd=="delete") cmd_delete(t,outputs);
        else if(cmd=="select") cmd_select(t,outputs);
        else if(cmd=="modify") cmd_modify(t,outputs);
        else if(cmd=="import") cmd_import(t,outputs);
        else if(cmd=="buy") cmd_buy(t,outputs);
        else if(cmd=="show"){
            if(t.size()>=2 && t[1]=="finance") { vector<string> tt; for(size_t i=2;i<t.size();++i) tt.push_back(t[i]); vector<string> o2; vector<string> t2; t2.push_back("show_financial"); if(tt.size()==1) t2.push_back(tt[0]); else if(tt.size()>1){ outputs.push_back("Invalid"); continue;} // we will call directly
            // call finance with manual parse
            vector<string> tf; tf.clear(); tf.push_back("show_finance"); if(tt.size()==1) tf.push_back(tt[0]); cmd_show_finance(tf, outputs);
            }
            else cmd_show(t,outputs);
        }
        else { outputs.push_back("Invalid"); }
    }
    // flush outputs
    for(size_t i=0;i<outputs.size();++i){ if(outputs[i].empty()) cout<<"\n"; else cout<<outputs[i]<<"\n"; }
    // persist
    saveAccounts(); saveBooks(); saveFinance();
    return 0; }

