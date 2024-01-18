<script setup lang="ts">
import router from '@/router'
import axios from 'axios'
import { ref, onMounted } from 'vue'
import { Plus, Select, Warning, Delete } from "@element-plus/icons-vue";
import { baseUrl } from '@/App'

const isLoading = ref(false)
const isSaveing = ref(false)
const activeSiteName = ref(0)

const formData = ref({
    enable: true,
    protocol: 'https',
    name: 'https_reverse_proxy_1',
    listen_address: '0.0.0.0',
    listen_port: "8888",
    ip_blacklist_minutes: "1440",
    cert_file: '',
    key_file: '',
    proxy_sites: [
        {
            name: "",
            domain: "",
            host: "127.0.0.1",
            port: "0",
            skip_body_for_head_request: true,
            skip_body_for_head_response: true,
            requires_auth: true,
            auth_roles: [
                {
                    method: "POST",
                    target: "",
                    result: "200"
                }
            ]
        }
    ]
})

const getConfig = async () => {
    if (isLoading.value) {
        return 102; // processing
    }

    isLoading.value = true;

    try {
        const res = await axios.get(baseUrl + '/api/config/http_reverse_proxy')

        isLoading.value = false;

        if (res.status == 200) {
            formData.value = res.data[0]
        }

        return res.status
    } catch (err) {
        isLoading.value = false;
        console.error(err)
        return 0
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
        const res = await axios.put(baseUrl + '/api/config/http_reverse_proxy',
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
    formData.value.proxy_sites.push({
        name: "",
        domain: "",
        host: "127.0.0.1",
        port: "0",
        skip_body_for_head_request: true,
        skip_body_for_head_response: true,
        requires_auth: true,
        auth_roles: [
            {
                method: "POST",
                target: "",
                result: "200"
            }
        ]
    })
}

const onDelSite = (index: number) => {
    formData.value.proxy_sites.splice(index, 1);
}

const onAddAuthRole = (index: number) => {
    formData.value.proxy_sites[index].auth_roles.push(
        {
            method: "POST",
            target: "",
            result: "200"
        }
    )
}

const onDelAuthRole = (index: number, idx: number) => {
    formData.value.proxy_sites[index].auth_roles.splice(idx, 1)
}
</script>

<template>
    <div class="stat" v-loading="isLoading">
        <div class="item">
            <div class="title">
                <el-icon>
                    <Warning />
                </el-icon>
                <span>代理参数配置</span>
            </div>
            <div class="content">
                <el-form :model="formData" label-width="120px">
                    <el-form-item label="">
                        <el-checkbox v-model="formData.enable" label="启用此服务" name="type" />
                    </el-form-item>
                    <el-form-item label="协议">
                        <el-select v-model="formData.protocol" placeholder="选择协议">
                            <el-option label="http" value="http" />
                            <el-option label="https" value="https" />
                        </el-select>
                    </el-form-item>
                    <el-form-item label="名称">
                        <el-input v-model="formData.name" />
                    </el-form-item>
                    <el-form-item label="监听地址">
                        <el-input v-model="formData.listen_address" />
                    </el-form-item>
                    <el-form-item label="监听端口">
                        <el-input v-model="formData.listen_port" />
                    </el-form-item>
                    <el-form-item label="SSL证书文件">
                        <el-tooltip effect="dark" content="类似 _.yourdomain.com-chain.pem 这样的文件,如果选择的是http协议则此处可不填写"
                            placement="bottom-start">
                            <el-input v-model="formData.cert_file" />
                        </el-tooltip>
                    </el-form-item>
                    <el-form-item label="SSL密钥文件">
                        <el-tooltip effect="dark" content="类似 _.yourdomain.com-key.pem 这样的文件,如果选择的是http协议则此处可不填写"
                            placement="bottom-start">
                            <el-input v-model="formData.key_file" />
                        </el-tooltip>
                    </el-form-item>
                    <el-form-item label="IP锁定">
                        <el-tooltip effect="dark" content="当密码输错3次之后,该IP要锁定多长时间禁止登录(单位分钟)" placement="bottom-start">
                            <el-input v-model="formData.ip_blacklist_minutes" />
                        </el-tooltip>
                    </el-form-item>
                </el-form>
            </div>
        </div>
        <div class="item">
            <div class="title">
                <el-icon>
                    <Warning />
                </el-icon>
                <span>代理站点配置</span>
                <el-button type="success" :icon="Plus" @click="onAddSite">新增站点</el-button>
            </div>
            <div class="content">
                <el-form :model="formData.proxy_sites" label-width="60px">
                    <el-collapse v-model="activeSiteName" accordion>
                        <el-collapse-item v-for="(item, index) in formData.proxy_sites" :key="index" :name="index">
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
                            <el-form-item label="名称">
                                <el-input v-model="item.name" />
                            </el-form-item>
                            <el-form-item label="域名">
                                <el-tooltip effect="dark" content="通常填写一个子域名,当代理收到此域名的请求时会转发给此站点对应的IP和端口去处理"
                                    placement="bottom-start">
                                    <el-input v-model="item.domain" />
                                </el-tooltip>
                            </el-form-item>
                            <el-form-item label="地址">
                                <el-tooltip effect="dark" content="此站点绑定的监听IP地址" placement="bottom-start">
                                    <el-input v-model="item.host" />
                                </el-tooltip>
                            </el-form-item>
                            <el-form-item label="端口">
                                <el-tooltip effect="dark" content="此站点绑定的监听端口" placement="bottom-start">
                                    <el-input v-model="item.port" />
                                </el-tooltip>
                            </el-form-item>
                            <el-form-item label="">
                                <el-checkbox v-model="item.skip_body_for_head_request" label="跳过HEAD请求体" name="type" />
                                <el-checkbox v-model="item.skip_body_for_head_response" label="跳过HEAD响应体" name="type" />
                            </el-form-item>
                            <el-form-item label="">
                                <div class="auth-role-title">
                                    <el-text tag="b" type="danger">登录验证规则</el-text>
                                    <el-tooltip effect="dark" content="新增规则" placement="bottom-start">
                                        <el-icon @click="onAddAuthRole(index)">
                                            <Plus />
                                        </el-icon>
                                    </el-tooltip>
                                </div>
                            </el-form-item>
                            <el-form-item label="">
                                <el-checkbox v-model="item.requires_auth" label="启用验证" name="type" />
                            </el-form-item>
                            <el-container v-for="(role, idx) in item.auth_roles" :key="idx">
                                <el-tooltip effect="dark" content="删除此验证规则" placement="bottom-start">
                                    <div class="delete" @click="onDelAuthRole(index, idx)">
                                        <el-icon>
                                            <Delete />
                                        </el-icon>
                                    </div>
                                </el-tooltip>
                                <el-form-item label="类型">
                                    <el-input v-model="role.method" />
                                </el-form-item>
                                <el-form-item label="目标">
                                    <el-input v-model="role.target" />
                                </el-form-item>
                                <el-form-item label="响应">
                                    <el-input v-model="role.result" />
                                </el-form-item>
                            </el-container>
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

                    .auth-role-title {
                        width: 100%;
                        margin-left: -40px;
                        display: flex;
                        flex-direction: row;
                        align-items: center;

                        .el-icon {
                            margin-left: auto;
                            margin-right: -4px;
                        }
                    }

                    .el-container {
                        display: flex;
                        flex-direction: column;
                        padding-right: 10px;
                        margin-left: 18px;
                        margin-bottom: 10px;
                        border: 1px solid #dedfe0;

                        .delete {
                            width: 30px;
                            height: 22px;
                            background-color: #fab6b6;
                            display: flex;
                            align-items: center;
                            justify-content: center;
                        }

                        .delete:hover {
                            background-color: #f89898;
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
