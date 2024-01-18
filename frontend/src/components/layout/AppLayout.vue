<script setup lang="ts">
import { RouterView } from 'vue-router'
import AppAside from './AppAside.vue'
import AppHead from './AppHead.vue';
</script>

<template>
    <div class="mainframe">
        <AppAside />
        <el-container>
            <el-container class="head-and-body">
                <AppHead />
                <el-main>
                    <el-scrollbar>
                        <RouterView />
                    </el-scrollbar>
                </el-main>
            </el-container>
        </el-container>
    </div>
</template>

<style scoped>
.mainframe {
    display: flex;
    flex-direction: row;
}

.el-main {
    background-color: #f4f4f5;
    padding-top: 0;
    padding-right: 0;
}

.head-and-body {
    flex-direction: column;
    height: 100vh;
}

@media screen and (max-width: 640px) {
    .el-main {
        padding-left: 0;
    }

    .el-aside {
        display: none;
    }
}
</style>
