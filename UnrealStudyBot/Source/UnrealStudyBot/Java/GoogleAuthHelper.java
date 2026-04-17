package com.studybot.game;

import android.app.Activity;
import android.content.Intent;
import android.util.Log;

import com.google.android.gms.auth.api.signin.GoogleSignIn;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInClient;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.tasks.Task;

/**
 * Google Sign-In JNI bridge for UE5 Android.
 *
 * C++ 진입점:
 *   FAndroidApplication::FindJavaClass("com/studybot/game/GoogleAuthHelper")
 *   GetStaticMethodID("signIn", "(Landroid/app/Activity;)V")
 *
 * Java → C++ 콜백:
 *   nativeOnIdToken(String idToken)       — 로그인 성공
 *   nativeOnGoogleSignInError(String msg) — 로그인 실패
 */
public class GoogleAuthHelper {

    private static final String TAG = "GoogleAuthHelper";
    private static final int RC_SIGN_IN = 9001;

    private static GoogleSignInClient sClient;
    private static Activity           sActivity;

    // ─── C++ → Java ──────────────────────────────────────────────────────────

    /**
     * Google Sign-In 플로우를 시작합니다.
     * UAuthSubsystem::LoginWithGoogle() 에서 JNI로 호출됩니다.
     *
     * @param activity GameActivity (FAndroidApplication::GetGameActivityThis())
     */
    public static void signIn(Activity activity) {
        sActivity = activity;

        GoogleSignInOptions gso = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
                .requestIdToken(activity.getString(R.string.google_web_client_id))
                .requestEmail()
                .build();

        sClient = GoogleSignIn.getClient(activity, gso);

        // 기존 세션 사이런트 로그인 시도 → 실패 시 계정 선택 팝업
        sClient.silentSignIn()
                .addOnSuccessListener(activity, account -> handleAccount(account))
                .addOnFailureListener(activity, e -> {
                    Log.d(TAG, "Silent sign-in failed, launching picker: " + e.getMessage());
                    Intent signInIntent = sClient.getSignInIntent();
                    activity.startActivityForResult(signInIntent, RC_SIGN_IN);
                });
    }

    /**
     * Google Sign-Out.
     * UAuthSubsystem::LogoutGoogle() 에서 JNI로 호출됩니다.
     *
     * @param activity GameActivity
     */
    public static void signOut(Activity activity) {
        GoogleSignInClient client = GoogleSignIn.getClient(
                activity,
                GoogleSignInOptions.DEFAULT_SIGN_IN
        );
        client.signOut().addOnCompleteListener(activity, task ->
                Log.d(TAG, "Google sign-out complete")
        );
    }

    // ─── onActivityResult 처리 ────────────────────────────────────────────────

    /**
     * GameActivity.onActivityResult() 에서 호출되어야 합니다.
     * UPL의 onActivityResult 훅을 통해 자동 연결됩니다.
     */
    public static void handleSignInResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != RC_SIGN_IN) return;

        Task<GoogleSignInAccount> task = GoogleSignIn.getSignedInAccountFromIntent(data);
        try {
            GoogleSignInAccount account = task.getResult(ApiException.class);
            handleAccount(account);
        } catch (ApiException e) {
            Log.w(TAG, "Google sign-in failed, code=" + e.getStatusCode());
            nativeOnGoogleSignInError("Google 로그인 실패 (code=" + e.getStatusCode() + ")");
        }
    }

    // ─── 내부 헬퍼 ───────────────────────────────────────────────────────────

    private static void handleAccount(GoogleSignInAccount account) {
        String idToken = account.getIdToken();
        if (idToken != null && !idToken.isEmpty()) {
            Log.d(TAG, "Got ID token, forwarding to native");
            nativeOnIdToken(idToken);
        } else {
            nativeOnGoogleSignInError("ID Token을 가져올 수 없습니다.");
        }
    }

    // ─── Java → C++ 네이티브 콜백 ────────────────────────────────────────────

    /**
     * Google ID Token을 C++ 레이어로 전달합니다.
     * C++ 구현: Java_com_studybot_game_GoogleAuthHelper_nativeOnIdToken
     */
    public static native void nativeOnIdToken(String idToken);

    /**
     * 로그인 실패 메시지를 C++ 레이어로 전달합니다.
     * C++ 구현: Java_com_studybot_game_GoogleAuthHelper_nativeOnGoogleSignInError
     */
    public static native void nativeOnGoogleSignInError(String errorMessage);
}
