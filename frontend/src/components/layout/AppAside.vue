<script setup lang="ts">
import { RouterView } from 'vue-router'
import { ref } from "vue";
import {
    House,
    Document,
    Operation,
    Fold,
    Monitor,
    Connection,
    UserFilled,
    Setting,
} from "@element-plus/icons-vue";
import { isMenuCollapse, allMenuInfos, activeMenuIndex, activeMenuTitle } from '@/App'

function handleMenuClick(index: string) {
    activeMenuTitle.value = "";
    for (let i = 0; i < allMenuInfos.length; i++) {
        if (allMenuInfos[i].index === index) {
            activeMenuTitle.value = allMenuInfos[i].title;
            activeMenuIndex.value = index;
            break;
        }
    }
}
</script>

<template>
    <el-aside>
        <el-scrollbar>
            <el-menu router :collapse="isMenuCollapse" :default-active="activeMenuIndex" @select="handleMenuClick">
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
                        <Setting />
                    </el-icon>
                    <span>{{ allMenuInfos[4].title }}</span>
                </el-menu-item>
            </el-menu>
        </el-scrollbar>
    </el-aside>
</template>

<style scoped>
.el-aside {
    background-color: #e9e9eb;
    height: 100vh;
    width: auto;
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
</style>
./SharedVariables