#ifndef _SYSCPUINFO_H
#define _SYSCPUINFO_H
#include <vector>
#include <string>
#include <memory>
#include "caps.h"
#include "SysCPUCoreInfo.h"
#include "SysCPUCoreInfo.h"
namespace rokid {
  /*
   * 系统CPU信息
   */
  class SysCPUInfo {
  private:
    std::shared_ptr<SysCPUCoreInfo> total = nullptr;
    std::shared_ptr<std::vector<SysCPUCoreInfo>> cores = nullptr;
  public:
    inline static std::shared_ptr<SysCPUInfo> create() {
      return std::make_shared<SysCPUInfo>();
    }
    /*
    * getter 总cpu信息
    */
    inline const std::shared_ptr<SysCPUCoreInfo> & getTotal() const {
      return total;
    }
    /*
    * getter CPU核心信息
    */
    inline const std::shared_ptr<std::vector<SysCPUCoreInfo>> getCores() const {
      return cores;
    }
    /*
    * setter 总cpu信息
    */
    inline void setTotal(const std::shared_ptr<SysCPUCoreInfo> &v) {
      total = v;
    }
    /*
    * setter CPU核心信息
    */
    inline void setCores(const std::shared_ptr<std::vector<SysCPUCoreInfo>> &v) {
      this->cores = v;
    }
    /*
     * serialize this object as buffer
    */
    int32_t serialize(void* buf, uint32_t bufsize) const;
    /*
     * deserialize this object as caps (with message type)
     */
    int32_t serialize(std::shared_ptr<Caps> &caps) const;
    /*
     * deserialize this object from buffer
     */
    int32_t deserialize(void* buf, uint32_t bufSize);
    /*
     * deserialize this object from caps (with message type)
     */
    int32_t deserialize(std::shared_ptr<Caps> &caps);
    /*
     * serialize this object as caps (without message type)
     */
    int32_t serializeForCapsObj(std::shared_ptr<Caps> &caps) const;
    /*
     * deserialize this object from caps (without message type)
     */
    int32_t deserializeForCapsObj(std::shared_ptr<Caps> &caps);
  };

}
#endif // _SYSCPUINFO_H