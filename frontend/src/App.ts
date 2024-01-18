import { ref } from "vue";

export const openSourceUrl = 'https://gitee.com/zhllxt/naslite';
export const baseUrl = '';

export const isMenuCollapse = ref(false);

export const activeMenuIndex = ref('');
export const activeMenuTitle = ref('');

export const allMenuInfos = [
    {
        "title": "http反向代理",
        "index": "/view/http_reverse_proxy"
    },
    {
        "title": "socks5反向代理",
        "index": "/view/socks5_reverse_proxy"
    },
    {
        "title": "web服务器",
        "index": "/view/static_http_server"
    },
    {
        "title": "进程管理",
        "index": "/view/service_process_mgr"
    },
    {
        "title": "系统设置",
        "index": "/view/frontend_http_server"
    }
];
