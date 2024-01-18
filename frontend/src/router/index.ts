import { createRouter, createWebHistory } from "vue-router";
import { useTokenStore } from "@/stores/UserToken";
import AppLayout from "../components/layout/AppLayout.vue";

const router = createRouter({
  history: createWebHistory(import.meta.env.BASE_URL),
  routes: [
    {
      path: "/",
      name: "",
      meta: { requiresAuth: true },
      component: AppLayout,
      children: [
        {
          path: "",
          name: "",
          meta: { requiresAuth: true },
          component: () => import("../views/IndexView.vue"),
        },
        {
          path: "/:xxx(.*)*",
          name: "ErrorPage",
          meta: { requiresAuth: true },
          component: () => import("../views/ErrorPage.vue"),
        },
        {
          path: "/view/http_reverse_proxy",
          name: "http_reverse_proxy",
          meta: { requiresAuth: true },
          component: () => import("../views/HttpReverseProxyView.vue"),
        },
        {
          path: "/view/service_process_mgr",
          name: "service_process_mgr",
          meta: { requiresAuth: true },
          component: () => import("../views/ServiceProcessMgrView.vue"),
        },
        {
          path: "/view/socks5_reverse_proxy",
          name: "socks5_reverse_proxy",
          meta: { requiresAuth: true },
          component: () => import("../views/Socks5ReverseProxyView.vue"),
        },
        {
          path: "/view/static_http_server",
          name: "static_http_server",
          meta: { requiresAuth: true },
          component: () => import("../views/StaticHttpServerView.vue"),
        },
        {
          path: "/view/frontend_http_server",
          name: "frontend_http_server",
          meta: { requiresAuth: true },
          component: () => import("../views/FrontendHttpServerView.vue"),
        },
      ],
    },
    {
      path: "/view/signin",
      name: "signin",
      component: () => import("../views/SigninView.vue"),
    },
  ],
});

router.beforeEach((to, from, next) => {
  //console.log("router.beforeEach.to: " + to.path);
  if (to.matched.some((r) => r.meta?.requiresAuth)) {
    const store = useTokenStore();
    if (!store.tokenObj.access_token) {
      next({ name: "signin" });
      return;
    }
  }
  next();
});

export default router;
