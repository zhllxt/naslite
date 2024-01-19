<script setup lang="ts">
import router from '@/router'
import axios from 'axios'
import { ref, onMounted } from 'vue'
import { Plus, Select, Warning, Delete } from "@element-plus/icons-vue";
import { baseUrl } from '@/App'

const isLoading = ref(false)
const isSaveing = ref(false)

const formData = ref({
    enable: true,
    protocol: 'http',
    name: 'frontend_http_server',
    listen_address: '0.0.0.0',
    listen_port: "8886",
    cert_file: '',
    key_file: '',
    webroot: "./admin_webroot",
    index: "index.html",
    max_request_header_size: "1048576",
    enable_cors: false,
    cors_allow_headers: "*",
    cors_allow_methods: "*",
    cors_allow_origin: "*",
    requires_auth: false,
    tokens: [
        {
            username: "admin",
            password: "123456",
            expires_at: "2035-01-01 00:00:00"
        }
    ]
})

const getConfig = async () => {
    if (isLoading.value) {
        return 102; // processing
    }

    isLoading.value = true;

    try {
        const res = await axios.get(baseUrl + '/api/config/frontend_http_server')

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
        const res = await axios.put(baseUrl + '/api/config/frontend_http_server',
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

const onAddToken = () => {
    formData.value.tokens.push(
        {
            username: "",
            password: "",
            expires_at: "2035-01-01 00:00:00"
        }
    )
}

const onDelToken = (index: number) => {
    formData.value.tokens.splice(index, 1)
}
</script>

<template>
    <div class="stat" v-loading="isLoading">
        <div class="item">
            <div class="title">
                <el-icon>
                    <Warning />
                </el-icon>
                <span>后台web管理参数配置</span>
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
                    <el-form-item label="网站根目录">
                        <el-tooltip effect="dark" content="可以填绝对路径也可以填相对路径,填相对路径时是相对于NasLite程序的路径" placement="bottom-start">
                            <el-input v-model="formData.webroot" />
                        </el-tooltip>
                    </el-form-item>
                    <el-form-item label="首页文件">
                        <el-tooltip effect="dark" content="通常为index.html这样的文件" placement="bottom-start">
                            <el-input v-model="formData.index" />
                        </el-tooltip>
                    </el-form-item>
                    <el-form-item label="">
                        <el-checkbox v-model="formData.enable_cors" label="允许跨域" name="type" />
                    </el-form-item>
                    <el-form-item label="跨域headers">
                        <el-tooltip effect="dark"
                            content="类似: Access-Control-Allow-Headers: * 或 Access-Control-Allow-Headers: x-requested-with,Content-Type,Authorization"
                            placement="bottom-start">
                            <el-input v-model="formData.cors_allow_headers" />
                        </el-tooltip>
                    </el-form-item>
                    <el-form-item label="跨域methods">
                        <el-tooltip effect="dark"
                            content="类似: Access-Control-Allow-Methods: * 或 Access-Control-Allow-Methods: GET,POST"
                            placement="bottom-start">
                            <el-input v-model="formData.cors_allow_methods" />
                        </el-tooltip>
                    </el-form-item>
                    <el-form-item label="跨域origin">
                        <el-tooltip effect="dark"
                            content="类似: Access-Control-Allow-Origin: * 或 Access-Control-Allow-Origin: http://localhost:5173"
                            placement="bottom-start">
                            <el-input v-model="formData.cors_allow_origin" />
                        </el-tooltip>
                    </el-form-item>
                    <el-form-item label="">
                        <el-checkbox v-model="formData.requires_auth" label="需要身份验证" name="type" />
                    </el-form-item>
                    <el-divider />
                    <el-form-item label="">
                        <div class="auth-role-title">
                            <el-text tag="b" type="danger">账号密码</el-text>
                            <el-tooltip effect="dark" content="新增一组账号密码" placement="bottom-start">
                                <el-icon @click="onAddToken()">
                                    <Plus />
                                </el-icon>
                            </el-tooltip>
                        </div>
                    </el-form-item>
                    <el-container v-for="(token, index) in formData.tokens" :key="index">
                        <el-tooltip effect="dark" content="删除此账号密码" placement="bottom-start">
                            <div class="delete" @click="onDelToken(index)">
                                <el-icon>
                                    <Delete />
                                </el-icon>
                            </div>
                        </el-tooltip>
                        <el-form-item label="账号">
                            <el-input v-model="token.username" />
                        </el-form-item>
                        <el-form-item label="密码">
                            <el-input type="password" show-password v-model="token.password" />
                        </el-form-item>
                        <el-form-item label="有效期">
                            <el-input v-model="token.expires_at" />
                        </el-form-item>
                    </el-container>
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

                .el-divider {
                    margin-left: 3px;
                }

                .auth-role-title {
                    width: 100%;
                    margin-left: -85px;
                    display: flex;
                    flex-direction: row;
                    align-items: center;

                    .el-icon {
                        margin-left: auto;
                        margin-right: -70px;
                    }
                }

                .el-container {
                    display: flex;
                    flex-direction: column;
                    padding-right: 10px;
                    margin-left: 10px;
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
