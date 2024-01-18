<script setup lang="ts">
import router from "@/router";
import axios from 'axios'
import { ref } from "vue";
import {
    Fold, Expand, UserFilled, CircleCloseFilled, ArrowDown, Link, RefreshLeft,
    Document,
    Operation,
    Monitor,
    Connection,
} from "@element-plus/icons-vue";
import imgAvatar from "@/assets/avatar.svg";
import imgMenu from "@/assets/menu.svg";
import { useTokenStore } from '@/stores/UserToken'
import { baseUrl, openSourceUrl, isMenuCollapse, allMenuInfos, activeMenuIndex, activeMenuTitle } from '@/App'
import '@/assets/index.css';

const store = useTokenStore()
const isRestarting = ref(false)
const showDrawer = ref(false)
const restarting = ref(null)

function togglePopDrawer() {
    showDrawer.value = !showDrawer.value;
}

function handleMenuClick(index: string) {
    activeMenuTitle.value = "";
    for (let i = 0; i < allMenuInfos.length; i++) {
        if (allMenuInfos[i].index === index) {
            activeMenuTitle.value = allMenuInfos[i].title;
            activeMenuIndex.value = index;
            break;
        }
    }
    showDrawer.value = false;
}

const handleCommand = (command: string | number | object) => {
    if (command == "signout") {
        store.clearToken();
        router.push("/view/signin")
    } else if (command == "open_source") {
        window.open(openSourceUrl, '_blank');
    }
}

function checkIsRestartFinished() {
    axios.get(baseUrl + "/api/user/signined")
        .then(res => {
            restarting.value.close()
        })
        .catch(err => {
            setTimeout(() => {
                checkIsRestartFinished();
            }, 1000)
        })
}

const onRestartNasLiteClicked = () => {
    ElMessageBox.confirm(
        '您确定要重启NasLite程序吗?',
        '提示',
        {
            confirmButtonText: '确定',
            cancelButtonText: '取消',
            type: 'warning',
        }
    )
        .then(() => {
            isRestarting.value = true
            axios.post(baseUrl + "/api/command/naslite/restart")
                .then(res => {
                    isRestarting.value = false
                    if (res.status == 200) {
                        ElMessage({
                            message: '重启NasLite程序成功,请等待重启完成',
                            type: 'success',
                        })

                        restarting.value = ElLoading.service({
                            lock: true,
                            text: '服务重启中...',
                            background: 'rgba(0, 0, 0, 0.7)',
                        })

                        setTimeout(() => {
                            checkIsRestartFinished();
                        }, 1000)

                    } else {
                        ElMessage({
                            message: '重启NasLite程序失败',
                            type: 'error',
                        })
                    }
                })
                .catch(err => {
                    isRestarting.value = false
                    console.error(err)
                    ElMessage({
                        message: '重启NasLite程序失败',
                        type: 'error',
                    })
                })
        })
        .catch(() => {
        })
}
</script>

<template>
    <el-drawer :size=220 custom-class="pop-drawer" v-model="showDrawer" direction="ltr" title="" :with-header="false">
        <el-scrollbar>
            <el-menu router :collapse="false" :default-active="activeMenuIndex" @select="handleMenuClick">
                <a href="/" class="logo">
                    <img src="@/assets/logo.png" />
                    <h1>NASLITE</h1>
                </a>
                <el-menu-item :index="allMenuInfos[0].index">
                    <el-icon>
                        <Operation />
                    </el-icon>
                    <span>{{ allMenuInfos[0].title }}</span>
                </el-menu-item>
                <el-menu-item :index="allMenuInfos[1].index">
                    <el-icon>
                        <Connection />
                    </el-icon>
                    <span>{{ allMenuInfos[1].title }}</span>
                </el-menu-item>
                <el-menu-item :index="allMenuInfos[2].index">
                    <el-icon>
                        <Monitor />
                    </el-icon>
                    <span>{{ allMenuInfos[2].title }}</span>
                </el-menu-item>
                <el-menu-item :index="allMenuInfos[3].index">
                    <el-icon>
                        <Document />
                    </el-icon>
                    <span>{{ allMenuInfos[3].title }}</span>
                </el-menu-item>
                <el-menu-item :index="allMenuInfos[4].index">
                    <el-icon>
                        <Document />
                    </el-icon>
                    <span>{{ allMenuInfos[4].title }}</span>
                </el-menu-item>
            </el-menu>
        </el-scrollbar>
    </el-drawer>

    <el-header>
        <el-icon @click="isMenuCollapse = !isMenuCollapse" v-show="isMenuCollapse">
            <Expand />
        </el-icon>
        <el-icon @click="isMenuCollapse = !isMenuCollapse" v-show="!isMenuCollapse">
            <Fold />
        </el-icon>
        <div @click="togglePopDrawer">
            <el-image :size="32" :src="imgMenu" />
        </div>

        <el-breadcrumb separator="/">
            <el-breadcrumb-item :to="{ path: '/' }">首页</el-breadcrumb-item>
            <el-breadcrumb-item>{{ activeMenuTitle }}</el-breadcrumb-item>
        </el-breadcrumb>

        <el-container>
            <el-tooltip effect="dark" content="重启NasLite程序" placement="bottom-start">
                <el-button type="warning" :icon="RefreshLeft" :loading="isRestarting" @click="onRestartNasLiteClicked"
                    circle />
            </el-tooltip>

            <el-dropdown @command="handleCommand">
                <span class="el-dropdown-link">
                    <el-avatar :size="32" :src="imgAvatar" />
                    <el-icon>
                        <ArrowDown />
                    </el-icon>
                </span>
                <template #dropdown>
                    <el-dropdown-menu>
                        <el-dropdown-item :icon="UserFilled" command="username">{{ store.tokenObj.username
                        }}</el-dropdown-item>
                        <el-dropdown-item :icon="Link" command="open_source">项目开源地址</el-dropdown-item>
                        <el-dropdown-item divided :icon="CircleCloseFilled" command="signout">退出登录</el-dropdown-item>
                    </el-dropdown-menu>
                </template>
            </el-dropdown>
        </el-container>
    </el-header>
</template>

<style scoped>
.el-drawer {
    background-color: #e9e9eb;
}

.el-menu {
    height: 100vh;
    width: 220px;
    background-color: #e9e9eb;
    border-right: none;

    &.el-menu--collapse {
        width: 60px;

        & h1 {
            display: none;
        }

        & .logo {
            justify-content: center;

            img {
                margin-left: 3px;
            }
        }
    }
}

.logo {
    display: flex;
    justify-content: left;
    align-items: center;
    text-decoration: none;
    color: black;
    height: 60px;

    img {
        margin-left: 20px;
        width: 32px;
        height: 32px;
    }

    h1 {
        margin-left: 10px;
        font-size: 18px;
        font-weight: bold;
    }
}

.el-header {
    display: flex;
    align-items: center;
    background-color: #dedfe0;

    .el-icon {
        margin-right: 12px;
    }

    .el-image {
        display: none;
        margin-left: -5px;
        margin-right: 10px;
        width: 24px;
        height: 24px;
    }

    .el-avatar {
        margin-right: 3px;
    }

    .el-breadcrumb {
        flex-grow: 1;
    }

    .el-container {
        display: flex;
        flex-direction: row;
        align-items: center;
        justify-content: right;

        .el-button {
            margin-right: 10px;
        }

        .el-dropdown {

            .el-dropdown-link {
                display: flex;
                align-items: center;
                justify-content: center;
            }
        }
    }
}

@media screen and (max-width: 640px) {
    .el-header {
        .el-icon {
            display: none;
        }

        .el-image {
            display: block;
        }
    }
}
</style>
./SharedVariables