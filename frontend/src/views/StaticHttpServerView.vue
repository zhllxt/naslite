<script setup lang="ts">
import router from '@/router'
import axios from 'axios'
import { ref, onMounted, watch } from 'vue'
import { Plus, Select, RefreshRight, Warning, Delete } from "@element-plus/icons-vue";
import { baseUrl } from '@/App'

const isLoading = ref(false)
const isSaveing = ref(false)
const activeSiteName = ref(0)

const formData = ref([
    {
        enable: true,
        protocol: "http",
        name: "",
        listen_address: "127.0.0.1",
        listen_port: "",
        cert_file: '',
        key_file: '',
        webroot: "./nav_webroot",
        index: "index.html",
        max_request_header_size: "1048576",
        enable_cors: false,
        requires_auth: false,
        tokens: []
    }
])

const getConfig = async () => {
    if (isLoading.value) {
        return 102; // processing
    }

    isLoading.value = true;

    try {
        const res = await axios.get(baseUrl + '/api/config/static_http_server')

        isLoading.value = false;

        if (res.status == 200) {
            formData.value = res.data
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

async function onSubmit() {
    if (isSaveing.value) {
        return;
    }
    isSaveing.value = true

    try {
        const res = await axios.put(baseUrl + '/api/config/static_http_server',
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

const onAddSite = () => {
    formData.value.push({
        enable: true,
        protocol: "http",
        name: "",
        listen_address: "127.0.0.1",
        listen_port: "",
        cert_file: '',
        key_file: '',
        webroot: "",
        index: "index.html",
        max_request_header_size: "1048576",
        enable_cors: false,
        requires_auth: false,
        tokens: []
    })
}

const onDelSite = (index: number) => {
    formData.value.splice(index, 1);
}
</script>

<template>
    <div class="stat" v-loading="isLoading">
        <div class="item">
            <div class="title">
                <el-icon>
                    <Warning />
                </el-icon>
                <span>静态站点配置</span>
                <el-button type="success" :icon="Plus" @click="onAddSite">新增站点</el-button>
            </div>
            <div class="content">
                <el-form :model="formData" label-width="100px">
                    <el-collapse v-model="activeSiteName" accordion>
                        <el-collapse-item v-for="(item, index) in formData" :key="index" :name="index">
                            <template #title>
                                <div class="title">
                                    <el-text type="primary">{{ index + 1 }}.</el-text>
                                    <el-text>{{ item.name }}</el-text>
                                    <el-tooltip effect="dark" content="删除此站点" placement="bottom-start">
                                        <el-icon @click.stop="onDelSite(index)">
                                            <Delete />
                                        </el-icon>
                                    </el-tooltip>
                                </div>
                            </template>
                            <el-form-item label="">
                                <el-checkbox v-model="item.enable" label="启用此站点" name="type" />
                            </el-form-item>
                            <el-form-item label="协议">
                                <el-select v-model="item.protocol" placeholder="选择协议">
                                    <el-option label="http" value="http" />
                                    <el-option label="https" value="https" />
                                </el-select>
                            </el-form-item>
                            <el-form-item label="名称">
                                <el-input v-model="item.name" />
                            </el-form-item>
                            <el-form-item label="地址">
                                <el-tooltip effect="dark" content="此站点绑定的监听IP地址" placement="bottom-start">
                                    <el-input v-model="item.listen_address" />
                                </el-tooltip>
                            </el-form-item>
                            <el-form-item label="端口">
                                <el-tooltip effect="dark" content="此站点绑定的监听端口" placement="bottom-start">
                                    <el-input v-model="item.listen_port" />
                                </el-tooltip>
                            </el-form-item>
                            <el-form-item label="SSL证书文件">
                                <el-tooltip effect="dark" content="类似 _.yourdomain.com-chain.pem 这样的文件,如果选择的是http协议则此处可不填写"
                                    placement="bottom-start">
                                    <el-input v-model="item.cert_file" />
                                </el-tooltip>
                            </el-form-item>
                            <el-form-item label="SSL密钥文件">
                                <el-tooltip effect="dark" content="类似 _.yourdomain.com-key.pem 这样的文件,如果选择的是http协议则此处可不填写"
                                    placement="bottom-start">
                                    <el-input v-model="item.key_file" />
                                </el-tooltip>
                            </el-form-item>
                            <el-form-item label="网站根目录">
                                <el-tooltip effect="dark" content="可以填绝对路径也可以填相对路径,填相对路径时是相对于NasLite程序的路径"
                                    placement="bottom-start">
                                    <el-input v-model="item.webroot" />
                                </el-tooltip>
                            </el-form-item>
                            <el-form-item label="首页文件">
                                <el-tooltip effect="dark" content="通常为index.html这样的文件" placement="bottom-start">
                                    <el-input v-model="item.index" />
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
</template>

<style scoped>
.stat {
    padding-top: 20px;
    display: flex;
    flex-direction: column;

    .item {
        width: 100%;
        max-width: 800px;
        margin: 5px;
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

            .el-icon {
                margin-left: 5px;
                margin-right: 5px;
            }

            .el-button {
                margin-left: auto;
                margin-right: 10px;
            }

            span {
                font-weight: bold;
            }
        }

        .content {
            background-color: #fff;
            padding-top: 10px;
            flex-grow: 1;

            .el-form {
                margin-right: 15px;

                .el-collapse {
                    margin-left: 8px;
                    margin-bottom: 16px;

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
