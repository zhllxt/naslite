<script setup lang="ts">
import router from '@/router'
import axios from 'axios'
import { ref, onMounted } from 'vue'
import { useTokenStore } from '@/stores/UserToken'
import { baseUrl } from '@/App'
import { Plus, Select, RefreshRight, Warning, Delete, Grid, Setting } from "@element-plus/icons-vue";

interface ProcessStatus {
  index: number
  name: string
  status: string
}

const store = useTokenStore()
const processStatusList = ref<ProcessStatus[]>([])
const isStarting = ref<boolean[]>([])
const isStoping = ref<boolean[]>([])
const isAllStarting = ref(false)
const isAllStoping = ref(false)
const isRefreshing = ref(false)
const isLoading = ref(false)
const isSaveing = ref(false)
const activeAppName = ref(0)

const formData = ref({
  enable: true,
  name: 'service_process_mgr_1',
  auto_start_process: true,
  auto_attach_process: true,
  stop_process_when_exit: false,
  stop_process_timeout: "5000",
  process_list: [
    {
      name: "App Name",
      path: "",
      args: "",
      childs: ""
    }
  ]
})

const getConfig = async () => {
  if (isLoading.value) {
    return 102; // processing
  }

  isLoading.value = true;

  try {
    const res = await axios.get(baseUrl + '/api/config/service_process_mgr')

    isLoading.value = false;

    if (res.status == 200) {
      formData.value = res.data[0]
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

const getAllProcessStatus = async () => {
  if (isLoading.value) {
    return 102; // processing
  }

  isLoading.value = true;

  try {
    const res = await axios.get(baseUrl + '/api/status/service_process_mgr/all_process')

    isLoading.value = false;

    if (res.status == 200) {
      processStatusList.value = res.data
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

onMounted(async () => {
  const result1 = await getAllProcessStatus()
  if (result1 == 401) {
    router.push("/view/signin")
    return
  }
  if (result1 != 200) {
    ElMessage({
      message: '加载进程信息失败',
      type: 'error'
    })
  }

  const result2 = await getConfig()
  if (result2 == 401) {
    router.push("/view/signin")
    return
  }
  if (result2 != 200) {
    ElMessage({
      message: '加载配置信息失败',
      type: 'error'
    })
  }
})

const onAddService = () => {
  formData.value.process_list.push({
    name: "App Name",
    path: "",
    args: "",
    childs: ""
  })
  activeAppName.value = formData.value.process_list.length - 1;
}

const onDelService = (index: number) => {
  formData.value.process_list.splice(index, 1);
}

async function onSubmit() {
  if (isSaveing.value) {
    return;
  }
  isSaveing.value = true

  try {
    const res = await axios.put(baseUrl + '/api/config/service_process_mgr',
      // params
      formData.value
    )
    if (res.status == 401) {
      router.push("/view/signin")
    } else if (res.status == 200) {
      ElMessage({
        message: '保存成功,重启NasLite程序后生效.',
        type: 'success'
      })
    } else {
      ElMessage({
        message: '保存失败',
        type: 'error'
      })
    }
  } catch (err) {
    ElMessage({
      message: '保存失败',
      type: 'error'
    })
  }

  isSaveing.value = false
}

async function onStartAll() {
  isAllStarting.value = true

  try {
    const res = await axios.post(baseUrl + '/api/command/service_process_mgr/start_all')
    if (res.status == 401) {
      router.push("/view/signin")
    } else if (res.status == 200) {
      ElMessage({
        message: '启动所有成功',
        type: 'success'
      })
      setTimeout(() => {
        getAllProcessStatus()
      }, 500)
    } else {
      ElMessage({
        message: '启动所有失败',
        type: 'error'
      })
    }
  } catch (err) {
    console.error(err)
    ElMessage({
      message: '启动所有失败',
      type: 'error'
    })
  }

  isAllStarting.value = false
}

async function onStopAll() {
  isAllStoping.value = true

  try {
    const res = await axios.post(baseUrl + '/api/command/service_process_mgr/stop_all')
    if (res.status == 401) {
      router.push("/view/signin")
    } else if (res.status == 200) {
      ElMessage({
        message: '停止所有成功',
        type: 'success'
      })
      setTimeout(() => {
        getAllProcessStatus()
      }, 500)
    } else {
      ElMessage({
        message: '停止所有失败',
        type: 'error'
      })
    }
  } catch (err) {
    console.error(err)
    ElMessage({
      message: '停止所有失败',
      type: 'error'
    })
  }

  isAllStoping.value = false
}

async function onRefresh() {
  isRefreshing.value = true
  const result = await getAllProcessStatus()
  if (result == 401) {
    router.push("/view/signin")
  } else if (result == 200) {
    ElMessage({
      message: '刷新成功',
      type: 'success'
    })
  } else {
    ElMessage({
      message: '刷新失败',
      type: 'error'
    })
  }
  isRefreshing.value = false
}

async function onStart(item, index) {
  isStarting.value[index] = true

  try {
    const res = await axios.post(baseUrl + '/api/command/service_process_mgr/start',
      // params
      {
        name: item.name
      }
    )
    if (res.status == 401) {
      router.push("/view/signin")
    } else if (res.status == 200) {
      item.status = 'running'
      ElMessage({
        message: '启动成功',
        type: 'success'
      })
    } else {
      ElMessage({
        message: '启动失败',
        type: 'error'
      })
    }
  } catch (err) {
    console.error(err)
    ElMessage({
      message: '启动失败',
      type: 'error'
    })
  }

  isStarting.value[index] = false
}

async function onStop(item, index) {
  isStoping.value[index] = true

  try {
    const res = await axios.post(baseUrl + '/api/command/service_process_mgr/stop',
      // params
      {
        name: item.name
      }
    )
    if (res.status == 401) {
      router.push("/view/signin")
    } else if (res.status == 200) {
      item.status = 'stopped'
      ElMessage({
        message: '停止成功',
        type: 'success'
      })
    } else {
      ElMessage({
        message: '停止失败',
        type: 'error'
      })
    }
  } catch (err) {
    console.error(err)
    ElMessage({
      message: '停止失败',
      type: 'error'
    })
  }

  isStoping.value[index] = false
}
</script>

<template>
  <el-tabs>
    <el-tab-pane>
      <template #label>
        <span class="custom-tabs-label">
          <el-icon>
            <Grid />
          </el-icon>
          <span>进程管理</span>
        </span>
      </template>
      <el-container v-loading="isLoading">
        <div class="main-button-container">
          <el-button type="primary" class="main-button" :loading="isAllStarting" @click="onStartAll">全部启动</el-button>
          <el-button type="danger" class="main-button" :loading="isAllStoping" @click="onStopAll">全部停止</el-button>
          <el-button type="success" class="main-button" :loading="isRefreshing" @click="onRefresh">刷 新</el-button>
        </div>
        <el-scrollbar>
          <el-descriptions v-for="(item, index) in processStatusList" :key="index" title="" :column="1" size="default"
            border>
            <el-descriptions-item :label="'' + Number(index + 1)" align="left" label-class-name="el-item-label">
              {{ item.name }}
            </el-descriptions-item>
            <el-descriptions-item label="" align="left" label-class-name="el-item-label">
              <div class="cell-item-content">
                <el-button plain type="primary" :loading="isStarting[index]" @click="onStart(item, index)">启动</el-button>
                <el-button plain type="danger" :loading="isStoping[index]" @click="onStop(item, index)">停止</el-button>
                <span v-if="item.status == 'running'" style="color: green">运行中</span>
                <span v-else-if="item.status == 'stopped'" style="color: red">已停止</span>
                <span v-else style="color: #888">未知</span>
              </div>
            </el-descriptions-item>
          </el-descriptions>
        </el-scrollbar>
      </el-container>
    </el-tab-pane>
    <el-tab-pane>
      <template #label>
        <span class="custom-tabs-label">
          <el-icon>
            <Setting />
          </el-icon>
          <span>参数配置</span>
        </span>
      </template>

      <div class="stat">
        <div class="item">
          <div class="title">
            <el-icon>
              <Warning />
            </el-icon>
            <span>全局参数</span>
          </div>
          <div class="content">
            <el-form :model="formData" label-width="80px">
              <el-form-item label="">
                <el-checkbox v-model="formData.enable" label="启用此服务" name="type" />
              </el-form-item>
              <el-form-item label="">
                <el-checkbox v-model="formData.auto_start_process" label="打开NasLite时自动启动所有服务" name="type" />
              </el-form-item>
              <el-form-item label="">
                <el-checkbox v-model="formData.auto_attach_process" label="打开NasLite时自动接管已启动的服务" name="type" />
              </el-form-item>
              <el-form-item label="">
                <el-checkbox v-model="formData.stop_process_when_exit" label="关闭NasLite时自动停止所有服务" name="type" />
              </el-form-item>
              <el-form-item label="超时">
                <el-tooltip effect="dark" content="停止进程时超过多长时间仍未结束后,强行杀死进程(单位毫秒)" placement="bottom-start">
                  <el-input v-model="formData.stop_process_timeout" />
                </el-tooltip>
              </el-form-item>
            </el-form>
          </div>
        </div>
      </div>

      <div class="stat">
        <div class="item">
          <div class="title">
            <el-icon>
              <Warning />
            </el-icon>
            <span>应用配置</span>
            <el-button type="success" :icon="Plus" @click="onAddService">新增应用</el-button>
          </div>
          <div class="content">
            <el-form :model="formData" label-width="100px">
              <el-collapse v-model="activeAppName" accordion>
                <el-collapse-item v-for="(item, index) in formData.process_list" :key="index" :name="index">
                  <template #title>
                    <div class="title">
                      <el-text type="primary">{{ index + 1 }}.</el-text>
                      <el-text>{{ item.name }}</el-text>
                      <el-tooltip effect="dark" content="删除此应用" placement="bottom-start">
                        <el-icon @click.stop="onDelService(index)">
                          <Delete />
                        </el-icon>
                      </el-tooltip>
                    </div>
                  </template>
                  <el-form-item label="名称">
                    <el-input v-model="item.name" />
                  </el-form-item>
                  <el-form-item label="路径">
                    <el-tooltip effect="dark" content="程序可执行文件的完整路径" placement="bottom-start">
                      <el-input v-model="item.path" />
                    </el-tooltip>
                  </el-form-item>
                  <el-form-item label="参数">
                    <el-tooltip effect="dark" content="程序启动时的附加参数" placement="bottom-start">
                      <el-input v-model="item.args" />
                    </el-tooltip>
                  </el-form-item>
                  <el-form-item label="子进程">
                    <el-tooltip effect="dark" content="某些应用打开后会同时启动一些子进程，在这里填写子进程的名称，多个子进程用分号分隔" placement="bottom-start">
                      <el-input v-model="item.childs" />
                    </el-tooltip>
                  </el-form-item>
                </el-collapse-item>
              </el-collapse>
            </el-form>
          </div>
        </div>
      </div>

      <div class="submit-div">
        <el-button type="primary" :loading="isSaveing" :icon="Select" @click="onSubmit">保 存</el-button>
      </div>
    </el-tab-pane>
  </el-tabs>
</template>

<style scoped>
.el-tabs {
  margin: 0px;
  padding: 0px;
}

.el-tabs>.el-tabs__content {
  padding: 32px;
  color: #6b778c;
  font-size: 32px;
  font-weight: 600;
}

.el-tabs .custom-tabs-label .el-icon {
  vertical-align: middle;
}

.el-tabs .custom-tabs-label span {
  vertical-align: middle;
  margin-left: 4px;
}

.stat {
  padding-top: 10px;
  display: flex;
  flex-direction: column;

  .item {
    width: 100%;
    max-width: 800px;
    margin: 4px;
    border: 1px solid #dedfe0;
    border-radius: 0px;
    background-color: #f4f4f5;
    display: flex;
    flex-direction: column;

    .title {
      display: flex;
      align-items: center;
      height: 42px;
      padding: 5px;
      border-bottom: 1px solid #dedfe0;

      .el-button {
        margin-left: auto;
      }

      .el-icon {
        margin-left: 5px;
        margin-right: 5px;
      }

      span {
        font-weight: bold;
      }
    }

    .content {
      width: 100%;
      background-color: #fff;
      padding: 10px;
      display: flex;
      align-items: center;
      justify-content: left;
      flex-grow: 1;

      .el-form {
        width: 100%;

        .el-collapse {
          width: 100%;

          .title {
            width: 100%;
            display: flex;
            flex-direction: row;
            align-items: center;
            border: none;

            .el-text {
              margin-right: 10px;
            }

            .el-icon {
              margin-left: auto;
              margin-right: 10px;
            }
          }
        }
      }
    }
  }
}

.submit-div {
  padding: 10px;
  width: 100%;
  max-width: 800px;
  display: flex;
  flex-direction: row;
  align-items: center;
  justify-content: center;

  .el-button {
    min-width: 180px;
  }
}

.el-container {
  display: flex;
  flex-direction: column;
  align-items: left;
  padding: 0px;
  margin: 4px;
  padding-top: 10px;
  background-color: #f1f1f1;
  height: 100%;

  .main-button-container {
    display: flex;
    flex-direction: row;
    align-items: center;
    margin-bottom: 10px;
    width: 100%;
    max-width: 400px;

    .main-button {
      width: 100%;
    }
  }

  .el-descriptions {
    margin-top: 8px;
    min-width: 320px;
    max-width: 400px;

    .el-button {
      width: 60px;
      margin: 3px;
      padding: 0px;
    }

    .cell-item-content {
      display: flex;
      flex-direction: row;
      align-items: center;

      span {
        text-align: right;
        flex-grow: 1;
      }
    }

    .el-item-label {
      width: 40px;
      min-width: 40px;
      max-width: 40px;
    }
  }
}

@media screen and (max-width: 640px) {
  .stat {
    padding-right: 10px;

    .item {
      .title {
        .el-button {
          margin-right: 0px;
        }
      }

      .content {
        .el-form {
          margin-right: 5px;
        }
      }
    }
  }
}
</style>
