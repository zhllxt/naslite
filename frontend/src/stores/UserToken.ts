import { ref, computed } from 'vue'
import { defineStore } from 'pinia'

interface Token {
  access_token: string
  token_type: string
  refresh_token: string
  expires_at: number
  username: string
}

export const useTokenStore = defineStore('UserToken', () => {
  const tokenStr = ref('')
  const tokenObj = computed<Token>(() => {
    return JSON.parse(tokenStr.value || window.localStorage.getItem('UserToken') || '{}')
  })

  function saveToken(data: string) {
    tokenStr.value = JSON.stringify(data)
    window.localStorage.setItem('UserToken', tokenStr.value)
  }

  function clearToken() {
    tokenStr.value = "{}"
    window.localStorage.removeItem('UserToken')
  }

  return { tokenObj, saveToken, clearToken }
})
