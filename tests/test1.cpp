#include <cstddef>
#include <nu.hpp>
#include <vector>

int main() {
  std::vector<std::byte> backing(1024*1024);

  nu::memory::Manager mgr(nu::memory::Slob, backing.data(), backing.size());
  nu::event::EventLoop loop(mgr);
  
  nu::python::Runtime rtn(mgr, loop);

  
  rtn.run("print('Hello World!')");
}
