import "./assets/main.css";

import { createApp } from "vue";
import { createPinia } from "pinia";

import App from "./App.vue";
import router from "./router";
import axios from "axios";
import { useTokenStore } from "@/stores/UserToken";

const app = createApp(App);

app.use(createPinia());
app.use(router);

const store = useTokenStore();

axios.interceptors.request.use(
  (config) => {
    //console.log("axios.interceptors.request, token: " + store.tokenObj.access_token);
    if (store.tokenObj.access_token) {
      config.headers.Authorization = store.tokenObj.access_token;
    }
    return config;
  },
  (error) => {
    return Promise.reject(error);
  }
);

axios.interceptors.response.use(
  response => {
    if (response.status === 401) {
      //console.log("axios.interceptors.response response.data.code == 401")
      router.push("/view/signin");
      return Promise.reject(response);
    } else {
      return response;
    }
  },
  error => {
    if (error.response) {
      if (error.response.status === 401) {
        //console.log("axios.interceptors.response error.response.status == 401")
        router.push("/view/signin");
        return Promise.reject(error);
      } else {
        return error.response;
      }
    } else {
      return Promise.reject(error);
    }
  }
);

app.mount("#app");
