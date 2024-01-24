<script setup lang="ts">
import router from "@/router";
import axios from 'axios'
import { ref, onMounted, onUnmounted } from "vue";
import { Warning, RefreshRight, SwitchButton } from "@element-plus/icons-vue";
import { useTokenStore } from '@/stores/UserToken'
import { baseUrl, activeMenuTitle } from '@/App'

const store = useTokenStore()
const isLoading = ref(false)
const isRestarting = ref(false)
const isShutdowning = ref(false)
const cpuTimerRef = ref(null);
const memTimerRef = ref(null);
const hardwareInfo = ref({
  cpu: "",
  memory: "",
  disk: ""
})
const cpuUsage = ref({
  percentage: "0"
})
const memUsage = ref({
  percentage: "0"
})
const diskUsage = ref([
  {
    name: "",
    total: "0",
    used: "0",
    percentage: "0"
  }
])
const hardwareTemperature = ref([
  {
    Type: "",
    Name: "",
    Val: 0,
    Max: 0
  }
])
const colors = [
  { color: '#6f7ad3', percentage: 20 },
  { color: '#1989fa', percentage: 40 },
  { color: '#5cb87a', percentage: 60 },
  { color: '#e6a23c', percentage: 80 },
  { color: '#f56c6c', percentage: 100 },
]

const get_hardware_info = async () => {
  if (isLoading.value) {
    return 102; // processing
  }

  isLoading.value = true;

  try {
    const res = await axios.get(baseUrl + '/api/status/device/hardware_info')

    isLoading.value = false;

    if (res.status == 200) {
      hardwareInfo.value = res.data
    }

    return res.status
  } catch (err) {
    isLoading.value = false;
    if (err.response && err.response.status) {
      return err.response.status;
    } else {
      console.error(err);
      return 0;
    }
  }
}

const get_disk_usage = async () => {
  if (isLoading.value) {
    return 102; // processing
  }

  isLoading.value = true;

  try {
    const res = await axios.get(baseUrl + '/api/status/device/disk_usage')

    isLoading.value = false;

    if (res.status == 200) {
      diskUsage.value = res.data
    }

    return res.status
  } catch (err) {
    isLoading.value = false;
    if (err.response && err.response.status) {
      return err.response.status;
    } else {
      console.error(err);
      return 0;
    }
  }
}

const get_cpu_usage = async () => {
  if (isLoading.value) {
    return 102; // processing
  }

  isLoading.value = true;

  try {
    const res = await axios.get(baseUrl + '/api/status/device/cpu_usage')

    isLoading.value = false;

    if (res.status == 200) {
      cpuUsage.value = res.data
    }

    return res.status
  } catch (err) {
    isLoading.value = false;
    if (err.response && err.response.status) {
      return err.response.status;
    } else {
      console.error(err);
      return 0;
    }
  }
}

const get_memory_usage = async () => {
  if (isLoading.value) {
    return 102; // processing
  }

  isLoading.value = true;

  try {
    const res = await axios.get(baseUrl + '/api/status/device/memory_usage')

    isLoading.value = false;

    if (res.status == 200) {
      memUsage.value = res.data
    }

    return res.status
  } catch (err) {
    isLoading.value = false;
    if (err.response && err.response.status) {
      return err.response.status;
    } else {
      console.error(err);
      return 0;
    }
  }
}

const get_hardware_temperatures = async () => {
  if (isLoading.value) {
    return 102; // processing
  }

  isLoading.value = true;

  try {
    const res = await axios.get(baseUrl + '/api/status/hardware/temperatures')

    isLoading.value = false;

    if (res.status == 200) {
      hardwareTemperature.value = res.data
    }

    return res.status
  } catch (err) {
    isLoading.value = false;
    if (err.response && err.response.status) {
      return err.response.status;
    } else {
      console.error(err);
      return 0;
    }
  }
}

function get_round_capacity(cap) {
  const n = Number(cap)
  if (n > 1099511627776)
    return (n / 1099511627776).toFixed(1) + 'T'
  else
    return (n / 1073741824).toFixed(0) + 'G'
}

onMounted(async () => {
  activeMenuTitle.value = "";

  const result1 = await get_hardware_info()
  if (result1 == 401) {
    router.push("/view/signin")
    return
  }
  if (result1 != 200) {
    ElMessage({
      message: '加载硬件信息失败',
      type: 'error'
    })
  }

  const result2 = await get_disk_usage()
  if (result2 == 401) {
    router.push("/view/signin")
    return
  }
  if (result2 != 200) {
    ElMessage({
      message: '加载硬盘使用率失败',
      type: 'error'
    })
  }

  const result3 = await get_cpu_usage()
  if (result3 == 401) {
    router.push("/view/signin")
    return
  }
  if (result3 != 200) {
    ElMessage({
      message: '加载CPU使用率失败',
      type: 'error'
    })
  }

  const result4 = await get_memory_usage()
  if (result4 == 401) {
    router.push("/view/signin")
    return
  }
  if (result4 != 200) {
    ElMessage({
      message: '加载内存使用率失败',
      type: 'error'
    })
  }

  const result5 = await get_hardware_temperatures()
  if (result5 == 401) {
    router.push("/view/signin")
    return
  }
  if (result5 != 200) {
    ElMessage({
      message: '加载硬件温度失败',
      type: 'error'
    })
  }

  cpuTimerRef.value = setInterval(async () => {
    await get_cpu_usage()
    await get_hardware_temperatures()
  }, 1500);

  memTimerRef.value = setInterval(async () => {
    await get_memory_usage()
  }, 10000);
})

onUnmounted(async () => {
  clearInterval(cpuTimerRef.value);
  clearInterval(memTimerRef.value);
})

const onRestartDeviceClicked = () => {
  ElMessageBox.confirm(
    '您确定要重启NAS服务器吗?',
    '提示',
    {
      confirmButtonText: '确定',
      cancelButtonText: '取消',
      type: 'warning',
    }
  )
    .then(() => {
      isRestarting.value = true
      axios.post(baseUrl + "/api/command/device/restart")
        .then(res => {
          isRestarting.value = false
          if (res.status == 200) {
            ElMessage({
              message: '重启NAS服务器成功,请等待重启完成',
              type: 'success',
            })
          } else {
            ElMessage({
              message: '重启NAS服务器失败',
              type: 'error',
            })
          }
        })
        .catch(err => {
          isRestarting.value = false
          console.error(err)
          ElMessage({
            message: '重启NAS服务器失败',
            type: 'error',
          })
        })
    })
    .catch(() => {
    })
}
const onShutdownDeviceClicked = () => {
  ElMessageBox.confirm(
    '您确定要关闭NAS服务器吗?',
    '提示',
    {
      confirmButtonText: '确定',
      cancelButtonText: '取消',
      type: 'warning',
    }
  )
    .then(() => {
      isShutdowning.value = true
      axios.post(baseUrl + "/api/command/device/shutdown")
        .then(res => {
          isShutdowning.value = false
          if (res.status == 200) {
            ElMessage({
              message: '关闭NAS服务器成功',
              type: 'success',
            })
          } else {
            ElMessage({
              message: '关闭NAS服务器失败',
              type: 'error',
            })
          }
        })
        .catch(err => {
          isShutdowning.value = false
          console.error(err)
          ElMessage({
            message: '关闭NAS服务器失败',
            type: 'error',
          })
        })
    })
    .catch(() => {
    })
}
</script>

<template>
  <div class="operation">
    <div class="row">
      <el-tooltip effect="dark" content="重启NAS服务器" placement="bottom-start">
        <el-button type="warning" :icon="RefreshRight" :loading="isRestarting"
          @click="onRestartDeviceClicked">重启</el-button>
      </el-tooltip>
      <el-tooltip effect="dark" content="关闭NAS服务器" placement="bottom-start">
        <el-button type="danger" :icon="SwitchButton" :loading="isShutdowning"
          @click="onShutdownDeviceClicked">关机</el-button>
      </el-tooltip>
      <div class="fillspace"></div>
    </div>
  </div>
  <div class="stat">
    <div class="item">
      <div class="title">
        <el-icon>
          <Warning />
        </el-icon>
        <span>硬件信息</span>
      </div>
      <div class="content">
        <el-descriptions title="" :column="1" size="default" border>
          <el-descriptions-item label="CPU" label-align="center" align="left" label-class-name="el-item-label">
            {{ hardwareInfo.cpu }}
          </el-descriptions-item>
          <el-descriptions-item label="内存" label-align="center" align="left" label-class-name="el-item-label">
            {{ hardwareInfo.memory }}
          </el-descriptions-item>
          <el-descriptions-item label="硬盘" label-align="center" align="left" label-class-name="el-item-label">
            {{ hardwareInfo.disk }}
          </el-descriptions-item>
        </el-descriptions>
      </div>
    </div>
    <div class="item">
      <div class="title">
        <el-icon>
          <Warning />
        </el-icon>
        <span>硬件温度</span>
      </div>
      <div class="content">
        <el-descriptions title="" :column="1" size="default" border>
          <el-descriptions-item v-for="(item, index) in hardwareTemperature" :key="index" label-align="left" align="center"
            label-class-name="el-item-label">
            <template #label>
              <el-tag>{{ item.Type }}</el-tag>
              {{ item.Name }}
            </template>
            {{ Math.round(Number(item.Val)) }}
          </el-descriptions-item>
        </el-descriptions>
      </div>
    </div>
    <div class="item">
      <div class="title">
        <el-icon>
          <Warning />
        </el-icon>
        <span>硬盘使用率</span>
      </div>
      <div class="content">
        <div class="disk">
          <div class="info" v-for="(item, index) in diskUsage" :key="index">
            <el-tag>{{ item.name }}</el-tag>
            <el-text>{{ get_round_capacity(item.total) }}</el-text>
            <el-progress :text-inside="false" :stroke-width="10" :percentage="Number(item.percentage)" :color="colors" />
          </div>
        </div>
      </div>
    </div>
    <div class="item">
      <div class="title">
        <el-icon>
          <Warning />
        </el-icon>
        <span>CPU使用率</span>
      </div>
      <div class="content">
        <el-progress type="dashboard" :percentage="Number(cpuUsage.percentage)" :color="colors" />
      </div>
    </div>
    <div class="item">
      <div class="title">
        <el-icon>
          <Warning />
        </el-icon>
        <span>内存使用率</span>
      </div>
      <div class="content">
        <el-progress type="dashboard" :percentage="Number(memUsage.percentage)" :color="colors" />
      </div>
    </div>
  </div>
</template>

<style scoped>
.stat {
  padding-top: 20px;
  display: flex;
  flex-wrap: wrap;
}

.operation {
  padding-top: 20px;
  display: flex;
  flex-wrap: wrap;
  align-items: space-between;
  flex-direction: row;

  .row {
    width: 100%;
    max-width: 400px;
    margin-left: 5px;
    margin-right: 5px;
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    flex-direction: row;

    .el-button {
      margin-top: 5px;
    }

    .fillspace {
      flex-grow: 1;
    }
  }
}

.item {
  width: 100%;
  max-width: 400px;
  margin: 5px;
  border: 1px solid #dedfe0;
  border-radius: 0px;
  background-color: #f4f4f5;
  display: flex;
  flex-direction: column;

  .title {
    display: flex;
    align-items: center;
    height: 36px;
    padding: 5px;
    border-bottom: 1px solid #dedfe0;

    .el-icon {
      margin-left: 5px;
      margin-right: 5px;
    }

    span {
      font-weight: bold;
    }
  }

  .content {
    background-color: #fff;
    padding: 10px;
    display: flex;
    align-items: center;
    justify-content: center;
    flex-grow: 1;

    .el-descriptions {
      width: 100%;

      .el-item-label {
        width: 40px;
        max-width: 40px;
      }
    }

    .disk {
      width: 100%;
      display: flex;
      flex-direction: column;
      align-items: space-between;
      justify-content: space-between;

      .info {
        display: flex;
        flex-direction: row;
        align-items: center;
        justify-content: center;
        margin-bottom: 3px;

        .el-tag {
          margin-right: 10px;
          width: 30px;
        }

        .el-text {
          width: 60px;
        }

        .el-progress--line {
          flex-grow: 1;
        }
      }
    }
  }
}
</style>
