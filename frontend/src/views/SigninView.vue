<script setup lang="ts">

import router from '@/router'
import { UserFilled, Lock } from '@element-plus/icons-vue'
import axios from 'axios'
import { ref, onMounted } from 'vue'
import { useTokenStore } from "@/stores/UserToken"
import { baseUrl, openSourceUrl } from '@/App'

const store = useTokenStore()
const input_username = ref('')
const input_password = ref('')
const isSignining = ref(false)

onMounted(() => {
  if (store.tokenObj.access_token) {
    axios.get(baseUrl + "/api/user/signined")
      .then(res => {
        if (res.status == 200) {
          router.push("/")
        }
      })
      .catch(err => {
      })
  }
})

function onSubmit() {
  if (isSignining.value) {
    return
  }

  isSignining.value = true

  axios.post(baseUrl + "/api/user/signin",
    // params
    {
      username: input_username.value,
      password: input_password.value,
    }
  )
    .then(res => {
      isSignining.value = false
      if (res.status == 200) {
        store.saveToken(res.data)
        router.push("/")
      } else {
        ElMessage({
          message: '账号或密码错误.',
          type: 'error',
        })
      }
    })
    .catch(err => {
      isSignining.value = false
      console.error(err)
      ElMessage({
        message: '账号或密码错误.',
        type: 'error',
      })
    })
}
</script>

<template>
  <div class="main-div">
    <div class="sub-div">
      <el-form>
        <el-container>
          <div class="title-div">
            <a :href="openSourceUrl" target="_blank">
              <h1>NASLITE</h1>
            </a>
          </div>
          <div>
            <el-input size="large" v-model="input_username" placeholder="账号">
              <template #prepend>
                <el-icon :size="20">
                  <UserFilled />
                </el-icon>
              </template>
            </el-input>
          </div>
          <div>
            <el-input size="large" v-model="input_password" type="password" placeholder="密码" show-password>
              <template #prepend>
                <el-icon :size="20">
                  <Lock />
                </el-icon>
              </template>
            </el-input>
          </div>
          <div>
            <el-button size="large" type="primary" :loading="isSignining" @click="onSubmit">登 录</el-button>
          </div>
        </el-container>
      </el-form>
    </div>
  </div>
</template>

<style scoped>
body {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 100vh;
  margin: 0;
  background-color: #f1f1f1;
}

.main-div {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background-color: #f1f1f1;
  height: 100vh;
  width: 100%;
}

.sub-div {
  justify-content: center;
  background-color: #fff;
  padding: 50px;
  border-radius: 10px;
}

.title-div {
  text-align: center;

  a {
    padding: 0;
  }

  h1 {
    font-size: 36px;
    margin-bottom: 30px;
  }
}

.el-container {
  display: flex;
  flex-direction: column;
  align-items: center;
}

.el-input {
  margin-bottom: 20px;
  width: 300px;
}

.el-button {
  margin-top: 20px;
  width: 300px;
}
</style>
